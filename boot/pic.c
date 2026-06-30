// boot/pic.c
# include "pic.h"

// PIC 분배기 하드웨어 초기화
// -> master IRQ(0-7)를 0x20(32번)로,
// slave IRQ(8-15)를 0x28(40번) 벡터로 강제 재배치
// Cascading

// 메인보드 I/O 포트로 1 byte를 직접 쏴주는 하드웨어 함수
void outb(uint16_t port, uint8_t val){
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

// 하드웨어 칩이 명령을 처리할 동안 미세하게 딜레이를 주는 안전 함수
void io_wait(void){
    outb(0x80, 0);
}

// I/O 포트에서 1바이트 읽어옴
// 읽은 결과는 AL 레지스터를 통해 value에 저장
uint8_t inb(uint16_t port){
    uint8_t value;

    __asm__ __volatile__("inb %1, %0" : "=a"(value) : "Nd"(port));

    return value;
}

void pic_init(void){
    // 초기화 시퀀스 시작 알림 (IC21)
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    // 핵심: 하드웨어 신호선 옮기기 (ICW2)
    // master PIC 신호(타이머, 키보드 등)은 32번(0x20)부터 순서대로 매핑
    outb(PIC1_DATA, 0x20);
    io_wait();
    //slave PIC 신호는 40번(0x28)부터 순서대로 매핑
    outb(PIC2_DATA, 0x28);
    io_wait();

    // master & slave 칩 간 하드웨어 연결 방식 설정 (ICW3)
    outb(PIC1_DATA, 4); // master의 2번 핀에 slave가 연결되었음을 알림(0100)
    // 2번 비트 -> 2번 핀
    io_wait();
    outb(PIC2_DATA, 2); // slave에게 자신이 master의 2번 핀에 연결되어 있음을 알림
    io_wait();

    // 프로세서 환경 모드 설정 (ICW4)
    outb(PIC1_DATA, ICW4_8086); // x86 모드로 최종 가동
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    // AIOS 격리 정책 준비
    // 우선 모든 하드웨어 신호를 완전 잠금
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);

}

// ICW1: 시작 알림
// 지금부터 리셋하고 세팅 시작할 테니 준비해라. 마지막에 ICW4 명령도 보낼 거다

// ICW2: 이정표 이사
// 이게 가장 중요함
// 하드웨어 신호가 오면 인텔 예외 구역(0~31) 피해서 32번, 40번 IDT 칸으로 점프해라

// ICW3: 배선 연결
//Master 2번 핀에 Slave 꼬리가 묶여 있으니 신호가 오면 그렇게 중계해라

// ICW4: 모드 고정
// 구형 프로세서가 아니라 x86 아키텍처 환경이니까 인텔 규격으로 일해라

void pic_mask_irq(uint8_t irq){
    uint16_t port;
    uint8_t bit;

    // IRQ 0-7은 master pic에 속함
    if(irq < 8){
        port = PIC1_DATA;
        bit = irq;
    }
    // IRQ 8-15는 salve pic에 속함
    else if(irq < 16){
        port = PIC2_DATA;
        bit = irq - 8;
    }
    else{
        return;
    }

    // 현재 mask 값ㅂ을 읽어옴
    uint8_t mask = inb(port);

    // 원하는 IRQ 비트를 1로 설정해서 차단
    mask |= (uint8_t)(1U << bit);

    outb(port, mask);
}

void pic_unmask_irq(uint8_t irq){
    uint16_t port;
    uint8_t bit;

    // IRQ 0-7은 master pic에 속함
    if(irq < 8){
        port = PIC1_DATA;
        bit = irq;
    }
    // IRQ 8-15는 salve pic에 속함
    else if(irq < 16){
        // slave의 pic의 irq를 열기 위해서는 master의 irq2 cascade도 열어야 함
        uint8_t master_mask = inb(PIC1_DATA);
        master_mask &= (uint8_t)~(1U << 2);
        outb(PIC1_DATA, master_mask);
        
        port = PIC2_DATA;
        bit = irq - 8;
    }
    else{
        return;
    }

    // 현재 mask를 읽고 원하는 IRQ 비트를 0으로 만들어서 허용
    uint8_t mask = inb(port);
    mask &= (uint8_t)~(1U << bit);
    outb(port, mask);
}