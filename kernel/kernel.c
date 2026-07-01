// kernel.c
# include "../boot/idt.h"
# include "../boot/pic.h"
# include "keyboard.h"
# include "arena.h"

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
// 64 bit 정수 주소값을 16진수 텍스트로 변환하여 사출하는 포맷터
static void vga_write_hex(int row, int column, uint64_t value, uint8_t attribute){
    char hex_str[19];
    const char* hex_digits = "0123456789ABCDEF";
    
    hex_str[0] = '0';
    hex_str[1] = 'x';
    
    // 64비트 주소값을 뒤에서부터 16자리로 파싱
    for (int i = 15; i >= 0; i--) {
        hex_str[2 + i] = hex_digits[value & 0x0F];
        value >>= 4;
    }
    hex_str[18] = '\0';
    
    vga_write_at(row, column, hex_str, attribute);
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

    // ================ arena test code =================
    
    // 아레나 가동
    arena_init();
    const char* arena_init_msg = "1. Tensor Arena Infrastructure Initialized.";
    vga_write_at(3, 0, arena_init_msg, 0x0E);
    
    // 가상의 임베딩/가중치 텐서 1 할당 (1000005 byte)
    // 일부러 정렬이 깨지도록 홀 수 크기를 할당
    void* tensor_weight_1 = arena_alloc(1000005);
    const char* arena_locked_msg_1 = "Tensor 1 (1MB) Locked at:";
    vga_write_at(4,2, arena_locked_msg_1, 0x07);
    if(tensor_weight_1 != NULL){
        vga_write_hex(4, 28, (uint64_t)tensor_weight_1, 0x0A);
    }
    else{
        vga_write_at(4, 28, "Allocating Failed", 0x4F);
    }

    // 연속해서 가상의 레이어 활성화 텐서 2 할당 (2000000 byte)
    void* tensor_weight_2 = arena_alloc(2000000);
    const char* arena_locked_msg_2 = "Tensor 2 (2MB) Locked at:";
    vga_write_at(5, 2,  arena_locked_msg_2, 0x07);
    if(tensor_weight_1 != NULL){
        vga_write_hex(5, 28, (uint64_t)tensor_weight_2, 0x0A);
    }
    else{
        vga_write_at(5, 28, "Allocating Failed", 0x4F);
    }

    // 의도적인 물리 메모리 고갈 한계 검증 (950MB)
    // arena 캡 한계(900MB)를 초과하므로 NULL 가드 터짐
    void* overflow_tensor = arena_alloc(950 * 1024 *1024);
    const char* arena_locked_msg_3 = "OOM Guard Safe Check    :";
    vga_write_at(6, 2, arena_locked_msg_3, 0x07);
    if(overflow_tensor == NULL){
        vga_write_at(6, 28, "PASS: out of memeory guard Locked", 0x0A);
    }
    else{
        vga_write_at(6, 28, "FAIL: Security Breach", 0x4F);
    }

    // ================ arena test code =================

    // 키보드 초기화 성공 표시
    vga_write_at(0, 55, "KBD: OK", 0x2F);

    // pic의 키보드 IRQ1 통로를 열어줌
    pic_unmask_irq(1);

    __asm__ __volatile__("sti");

    while(1){
        __asm__ __volatile__("hlt");
    }
}