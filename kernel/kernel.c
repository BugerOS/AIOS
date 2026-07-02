// kernel.c
# include "../boot/idt.h"
# include "../boot/pic.h"
# include "keyboard.h"
# include "arena.h"
# include "matrix.h"

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

// vga 화면에 float 실수를 사출하는 포맷터
static void vga_write_float(int row, int column, float value, uint8_t attribute){
    char buf[16];
    int idx = 0;

    if(value < 0){
        buf[idx++] = '-';
        value = -value;
    }

    // 정수부 분리
    int int_part = (int)value;

    // 소수부분 분리(일단 소수점 2자리까지만)
    int frac_part = (int)((value - int_part) * 100.0f + 0.5f);

    // 정수 부분도 단순 2자리만 일단
    if(frac_part >= 100){
        int_part += 1;
        frac_part -= 100;
    }
    
    // 정수 부분 가변 자릿수 동적 카운팅(이건 역순)
    int temp_int = int_part;
    int start_int_idx = idx;
    do{
        buf[idx++] = '0' + (temp_int %10);
        temp_int /= 10;
    }while(temp_int > 0);

    // 역순이었으니 다시 뒤집기
    int end_int_idx = idx - 1;
    while(start_int_idx < end_int_idx){
        char temp = buf[start_int_idx];
        buf[start_int_idx] = buf[end_int_idx];
        buf[end_int_idx] = temp;
        start_int_idx++;
        end_int_idx--;
    }

    buf[idx++] = '.';
    buf[idx++] = '0' + (frac_part / 10);
    buf[idx++] = '0' + (frac_part % 10);
    buf[idx++] = '\0';

    vga_write_at(row, column, buf, attribute);

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
    
    // ---------------- matrix test code ----------------

    const char* matrix_msg = "2. Launching Cache-Friendly Matrix Engine...";
    vga_write_at(5, 0, matrix_msg, 0x0E);

    // 2x3 행렬 A 생성
    matrix_t* A = matrix_create(2,3);
    matrix_set(A, 0,0,1.0f);
    matrix_set(A, 0,1,2.0f);
    matrix_set(A, 0, 2, 3.0f);
    matrix_set(A, 1, 0, 4.0f);
    matrix_set(A, 1, 1, 5.0f);
    matrix_set(A, 1, 2, 6.0f);

    // 3x2 행렬 B 생성
    matrix_t* B = matrix_create(3, 2);
    matrix_set(B, 0, 0, 7.0f);
    matrix_set(B, 0, 1, 8.0f);
    matrix_set(B, 1, 0, 9.0f);
    matrix_set(B, 1, 1, 10.0f);
    matrix_set(B, 2, 0, 11.0f);
    matrix_set(B, 2, 1, 12.0f);

    // A x B 를 위한 C 분할 선점
    matrix_t* C = matrix_create(2,2);

    // 런타임 연산 무결성 덤프 출력
    if(matrix_mul(A,B,C)){
        vga_write_at(7, 2, "[Matrix Multiplication Result (2x2)]", 0x0A);

        float res_00, res_01, res_10, res_11;
        matrix_get(C, 0, 0, &res_00);
        matrix_get(C, 0, 1, &res_01);
        matrix_get(C, 1, 0, &res_10);
        matrix_get(C, 1, 1, &res_11);

        vga_write_at(9,  4, "Row 0: ", 0x07);
        vga_write_float(9,  12, res_00, 0x02);
        vga_write_float(9,  22, res_01, 0x02);

        vga_write_at(10, 4, "Row 1: ", 0x07);
        vga_write_float(10, 12, res_10, 0x02);
        vga_write_float(10, 22, res_11, 0x02);

        vga_write_at(12, 2, "STATUS: SSE2 Cache-Friendly Execution PASS.", 0x0F);
    } else {
        vga_write_at(7, 2, "STATUS: Matrix Execution CRITICAL ERROR.", 0x4F);
    }
    

    // --------------------------------------------------
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