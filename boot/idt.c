// boot/idt.c
# include "idt.h"
# include <stdint.h>

// x86_64 아키텍처는 최대 256개의 interrupt vector를 가짐
# define IDT_MAX_ENTRIES 256

// boot.S의 GDT에서 0x08은 64 bit 커널 코드 세그먼트
# define KERNEL_CODE_SELECTION 0x08

// present + ring0 + 64bit interrupt gate
# define IDT_INTERRUPT_GATE 0x8E

// PIC master의 시작 벡터는 0x20
# define PIC_MASTER_OFFSET 0x20

// ps/2 키보드는 IRQ1 사용
# define KEYBOARD_IRQ 1

// 키보드가 들어갈 최종 IDT 번호
# define KEYBOARD_VECTOR (PIC_MASTER_OFFSET + KEYBOARD_IRQ)

// 16 바이트 이정표 256개를 메모리에 실제로 연속 배치
// aligned(0x10)은 16 byte 단위 정렬
__attribute__((aligned(0x10))) static struct idt_entry idt[IDT_MAX_ENTRIES];
static struct idt_ptr idtr;

extern void isr_default_wrapper(void);
extern void isr_keyboard_wrapper(void);

// 특정 인터럽트 번호에 함수 주소를 주입하는 내부 헬퍼 함수
// 64 bit 가상 주소를 하드웨어가 요구하는 조건으로 쪼개서 이정표에 바인딩하는 함수
void idt_set_gate(uint8_t vector, void (*isr)(void), uint8_t attributes){
    uint64_t address = (uint64_t) isr;

    idt[vector].isr_low = (uint16_t)(address & 0xFFFF); // and를 통해서 뒤쪽 16 비트만 뽑아냄
    idt[vector].kernel_cs = KERNEL_CODE_SELECTION; //boot.S에서 뚫어놓은 64 bit code segment 고속도로 번호
    idt[vector].ist = 0;
    idt[vector].attributes = attributes;
    idt[vector].isr_mid = (uint16_t)((address >> 16) & 0xFFFF);
    idt[vector].isr_high = (uint32_t)((address >> 32) & 0xFFFFFFFF);
    idt[vector].reserved = 0;
}

// // test용 임시 이정표 함수 (추후 assembly handler로 대체 예정)
// void dummy_isr(void){
//     // interrupt가 정상 작동하는지 확인하기 위해서 cpu 정지
//     // 예기치 못한 하드웨어 충돌 시 인터럽트를 차단(cli), cpu 정지(hlt)
//     __asm__ __volatile__("cli; hlt");
// }

// kernel 본체에서 최초 실행될 IDT 시스템 가동 함수
// kernel.c에서 호출 -> cpu를 AIOS의 제어판으로 대체
void idt_init(void){
    // 우선 256개 핸들러 전체를 dummy 함수로 대체 -> 예기치 못한 GP fault 등으로 인한 즉사를 방지
    // GP fault: 하드웨어 규칙 위반 경고 및 예외
    // GP fault: 존재하지 않는 메모리 주소를 건드리거나, 권한이 없는 곳(Ring3)에서 특권 명령(Ring0)를 실행하는 등
    for(int i=0; i<IDT_MAX_ENTRIES; i++){
        idt_set_gate((uint8_t)i, isr_default_wrapper, IDT_INTERRUPT_GATE); // 0x8E는 현재 커널 모드(Ring0)에서만 작동하는 interrupt gate
    }

    // dummy_isr이 아닌 진짜 키보드 드라이버 래퍼로
    idt_set_gate(KEYBOARD_VECTOR, isr_keyboard_wrapper, IDT_INTERRUPT_GATE);
    
    // CPU가 읽어갈 포인터 등록 명세
    idtr.limit = (uint16_t)(sizeof(idt) - 1); // cpu 규격상 limit는 마지막 유효 바이트의 offset
    idtr.base = (uint64_t)&idt[0]; // idt 배열의 시작 주소

    // 하드웨어 명령어 조작: cpu 레지스터에 새로운 IDT(Interrupt Descriptor Table) 로드
    // 외부 자극에 따라서 cpu가 반사적으로 행동할 수 있도록 미리 심어두는 하드웨어 주소 이정표
    __asm__ __volatile__("lidt %0" : : "m"(idtr));
    // extended inline assembly 문법
    // "lidt %0": Load Interrupt Descriptor Table -> 실행할 실제 cpu 기계어 명령
    // :는 구분
    // 중간의 공백은 출력 파라미터가 들어갈 자리(현재 없음)
    // "m"(idtr): 입력 파라미터 명세(m은 mem)
}