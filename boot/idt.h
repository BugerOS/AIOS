// boot/idt.h
# ifndef IDT_H
# define IDT_H
# include <stdint.h>

// 64 bit IDT gate discriptor structure (16 byte == 128 bit 표준에 따름)
struct idt_entry{
    uint16_t isr_low;   // 0-15 bit:    ISR(인터럽트 함수) 주소의 하위 16 bit
    uint16_t kernel_cs; // 16-31 bit:   kernel code segment selector (0x08)
    uint8_t ist;        // 32-39 bit:   interrupt stack table(사용 x, 0으로 설정)
    uint8_t attributes; // 40-47 bit:   권한 및 게이트 타입 설정 (present + ring0 +interrupt gate = 0x8E)
    uint16_t isr_mid;   // 48-63 bit:   ISR 주소의 중간 16 bit
    uint32_t isr_high;  // 64-95 bit:   ISR 주소의 상위 32 bit
    uint32_t reserved;  // 96-127 bit:  하드웨어 규정 상 무조건 비워놔야 하는 예약 영역 (0)
} __attribute__((packed));

// cpu register에 IDT 위치를 넘겨주기 위한 pointer 구조체 (총 10 byte 표준 명세)
// cpu의 물리 레지스터인 IDTR에 제어판이 메모리 어디에 있고, 크기가 얼마인지 넘겨주기 위한 주소록
struct idt_ptr{
    uint16_t limit; // IDT table 전체 크기 - 1
    uint64_t base; // IDT table 시작 물리 주소
} __attribute__((packed));

// 외부에서 호출할 interrupt 초기화 함수 interface
void idt_init(void);

# endif