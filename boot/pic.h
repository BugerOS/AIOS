// boot/pic.h
// 신호 재배치 엔진
// 충돌이 나도록 엉망으로 꼬여있는 옛날 하드웨어 배선 규격을 32번(0x20) 이후의 깨끗한 안전 구역으로
# ifndef PIC_H
# define PIC_H
# include <stdint.h>

// master PIC와 slave PIC 칩이 바라보는 메인보드 I/O 포트 주소
#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI 0x20

// PIC 초기화 및 제어를 위한 ICW(Initialization Command Word) 플래그
#define ICW1_INIT 0x10 // PIC에게 초기화 명령 시작을 알림
#define ICW1_ICW4 0x01 // ICW4(포맷 설정) 명령이 뒤따라올 것임을 명시
#define ICW4_8086 0x01 // 8086/8088 (x86) 모드로 하드웨어를 구동

void outb(uint16_t port, uint8_t value); // master PIC에 EOI를 전송
// EOI: end of interrupt -> interrupt 처리의 끝을 알림
void io_wait(void); // 짧은 I/O 지연 함수

uint8_t inb(uint16_t port); // cpu에서 I/O 포트의 1 바이트 읽음

// 외부 kernel에서 호출할 PIC 가동 interface
void pic_init(void); // 초기화 직후에는 모든 IRQ 차단

void pic_mask_irq(uint8_t irq); // 지정한 IRQ 차단
void pic_unmask_irq(uint8_t irq); // 지정한 IRQ를 허용

# endif