// kernel.c
# include "../boot/idt.h"
# include "../boot/pic.h"
# include "keyboard.h"

// VGA 텍스트 모드의 물리 메모리 주소 (화면 출력 버퍼)
# define VIDEO_MEMORY 0xB8000
# define WIDTH 80
# define HEIGHT 25

static void vga_clear(void){
    volatile uint16_t* video_buffer = (volatile uint16_t*)VIDEO_MEMORY;

    for(int i = 0; i < WIDTH*HEIGHT; i++){
        video_buffer[i] = ((uint16_t)0x0F << 8) | ' ';
        // 0x0F는 검은 화면에 흰색
    }
}

// 지정한 행과 열부터 문자열 출력
static void vga_write_at(int row, int column, const char* text, uint8_t attribute){
    volatile uint16_t* video_buffer = (volatile uint16_t*)VIDEO_MEMORY;

    int position = row *WIDTH +column;

    for(int i = 0; text[i] != '\0'; i++){
        if(position + i >= WIDTH*HEIGHT){
            break;
        }

        video_buffer[position + i] = ((uint16_t)attribute << 8) | (uint8_t)text[i];

    }
}

// bios나 grub가 남긴 vga 하드웨어 커서를 숨김
static void vga_disable_hardware_cursor(void){
    // vga crt controller의 cursor start register 선택
    outb(0x3D4, 0x0A);

    // bit 5를 1로 설정 -> 하드웨어 커서 비활성화됨
    outb(0x3D5, 0x20);
}

void kernel_main(void){

    __asm__ __volatile__("cli");
    vga_clear();
    vga_disable_hardware_cursor();

    const char* status_label = "AIOS Kernel System | IO_STAT: 0x   | INT_IN:";
    vga_write_at(0, 0, status_label, 0x1F);

    const char* msg = "Welcome to AIOS! 64-bit Kernel Successfully Loaded.";
    vga_write_at(1, 0, msg, 0x0F);
    // unsigned short* video_buffer = (unsigned short*)VIDEO_MEMORY;
    
    // 64 bit 인터럽트 통제판 등록
    // idt 깔기
    idt_init();

    // pic 리매핑
    // pic: 메인모드 신호 분배기
    pic_init();

    if(!keyboard_init()){
        const char* error_msg = "ERROR: PS/2 keyboard initialization failed.";
        vga_write_at(3, 0, error_msg, 0x4F);

        // 초기화 실패 시 인터럽트 열지 않고 정지
        while(1){
            __asm__ __volatile__("cli; hlt");
        }
    }

    // 키보드 초기화 성공 표시
    vga_write_at(0, 55, "KBD: OK", 0x2F);

    // pic의 키보드 IRQ1 통로를 열어줌
    pic_unmask_irq(1);

    __asm__ __volatile__("sti");

    while(1){
        __asm__ __volatile__("hlt");
    }
}