// kernel/ekyboard.c
// 하드웨어 포트 파싱 및 그래픽 메모리 덤프

/*
    첫 번째 줄: 상태 표시
    두 번째 줄: 환영 메시지
    세 번째 줄: 키보드 입력
*/
# include <stdint.h>
# include "keyboard.h"
# include "../boot/pic.h"

# define VGA_MEMORY 0XB8000
# define VGA_WIDTH 80
# define VGA_HEIGHT 25

# define INPUT_START_CELL (VGA_WIDTH*2)
# define STATUS_SCANCODE_CELL 32
# define STATUS_COUNTER_CELL 44

# define PS2_DATA_PORT 0x60
# define PS2_STATUS_PORT 0x64
# define PS2_COMMAND_PORT 0x64

# define PS2_TIMEOUT 1000000U

static uint16_t cursor_cell = INPUT_START_CELL;
static uint32_t interrupt_count = 0;
static int extended_scancode = 0;

/*
    키보드를 누르면 A, B와 같은 글자를 주는 것이 아니라
    메인보드 회로상의 위치 번호를 줌
    index 0x10(16) 자리에 q, 0x11(17) 자리에 w가 배치되는 아키텍처 표준 배열
*/
static const char scancode_to_ascii[128] = {
    [0x01] = 27,

    [0x02] = '1',
    [0x03] = '2',
    [0x04] = '3',
    [0x05] = '4',
    [0x06] = '5',
    [0x07] = '6',
    [0x08] = '7',
    [0x09] = '8',
    [0x0A] = '9',
    [0x0B] = '0',
    [0x0C] = '-',
    [0x0D] = '=',
    [0x0E] = '\b',
    [0x0F] = '\t',

    [0x10] = 'q',
    [0x11] = 'w',
    [0x12] = 'e',
    [0x13] = 'r',
    [0x14] = 't',
    [0x15] = 'y',
    [0x16] = 'u',
    [0x17] = 'i',
    [0x18] = 'o',
    [0x19] = 'p',
    [0x1A] = '[',
    [0x1B] = ']',
    [0x1C] = '\n',

    [0x1E] = 'a',
    [0x1F] = 's',
    [0x20] = 'd',
    [0x21] = 'f',
    [0x22] = 'g',
    [0x23] = 'h',
    [0x24] = 'j',
    [0x25] = 'k',
    [0x26] = 'l',
    [0x27] = ';',
    [0x28] = '\'',
    [0x29] = '`',

    [0x2B] = '\\',
    [0x2C] = 'z',
    [0x2D] = 'x',
    [0x2E] = 'c',
    [0x2F] = 'v',
    [0x30] = 'b',
    [0x31] = 'n',
    [0x32] = 'm',
    [0x33] = ',',
    [0x34] = '.',
    [0x35] = '/',

    [0x37] = '*',
    [0x39] = ' '
};

/*
    inb 하드웨어 명령어 인라인 매핑
    outb: 밖으로 쏴주는 것
    inb: Input Byte
    메인보드의 특정 칩셋의 데이터를 pu로 읽어오는 기계어
*/
// static inline uint8_t inb(uint16_t port){
//     uint8_t value;
//     __asm__ __volatile__("inb %1, %0" : "=a"(value) : "Nd"(port));
//     // "=a"(ret): 읽어온 결과 데이터는 al/eax 레지스터에 자동으로 할당됨
//     // al/eax는 cpu 연산ㄴ의 핵심
//     // "Nd"(port): 타겟이 될 하드웨어 포트번호(ox60)는 dx 레지스터에 담겨서 전달됨
//     return value;
// }

static int ps2_wait_input_empty(void){
    for(uint32_t i = 0; i < PS2_TIMEOUT; i++){
        // 상태 포트 bit 0 -> 컨트롤러 입력 버퍼가 비어 있어서 명령/데이터 쓸 수 있음
        if((inb(PS2_STATUS_PORT) & 0x02) == 0){
            return 1;
        }
    }
    return 0;
}

static int ps2_wait_output_full(void){
    for(uint32_t i = 0; i < PS2_TIMEOUT; i++){
        if(inb(PS2_STATUS_PORT) & 0x01){
            return 1;
        }
    }
    return 0;
}

static int ps2_write_command(uint8_t command){
    if(!ps2_wait_input_empty()){
        return 0;
    }
    outb(PS2_COMMAND_PORT, command);
    return 1;
}

static int ps2_write_data(uint8_t data){
    if(!ps2_wait_input_empty()){
        return 0;
    }
    outb(PS2_DATA_PORT, data);
    return 1;
}

static int ps2_read_data(uint8_t* value){
    if(value==0){
        return 0;
    }

    if(!ps2_wait_output_full()){
        return 0;
    }
    *value = inb(PS2_DATA_PORT);
    return 1;
}

static void ps2_flush_output(void){
    // BIOS나 grub가 남긴 데이터 및 ack 제거
    for(int i = 0; i<64; i++){
        if((inb(PS2_STATUS_PORT) & 0x01) == 0){
            break;
        }

        (void)inb(PS2_DATA_PORT);
    }
}

// 키보드가 명령을 보내고 ack(0xFA)를 기다림
// 0xFE가 오면 명령 재전송
static int keyboard_send_command(uint8_t command){
    for(int attempt = 0; attempt < 3; attempt++){
        uint8_t response;

        if(!ps2_write_data(command)){
            return 0;
        }

        if(!ps2_read_data(&response)){
            return 0;
        }

        if(response == 0xFA){
            return 1;
        }

        if(response != 0xFE){
            return 0;
        }
    }
    return 0;
}

static void status_show_hex(uint8_t value){
    const char hex_digits[] = "0123456789ABCDEF";
    volatile char* vga = (volatile char*)VGA_MEMORY;

    vga[STATUS_SCANCODE_CELL] = ((uint16_t)0x0B << 8) | (uint8_t)hex_digits[(value >> 4) & 0x0F];

    vga[STATUS_SCANCODE_CELL + 1] = ((uint16_t)0x0B << 8) | (uint8_t)hex_digits[value & 0x0F];

}

static void status_show_counter(uint32_t count){
    volatile uint16_t* vga = (volatile uint16_t*)VGA_MEMORY;
    
    if(count > 99999U){
        count%=100000U;
    }

    for(int i=4; i>=0; i--){
        uint8_t digit = (uint8_t)(count % 10U);

        vga[STATUS_COUNTER_CELL + i] = ((uint16_t)0x0E << 8) | (uint16_t)('0' + digit);

        count/=10U;
    }
}

static void vga_put_char(char c){
    volatile uint16_t* vga = (volatile uint16_t*)VGA_MEMORY;

    if(c == '\n'){
        cursor_cell += VGA_WIDTH - (cursor_cell % VGA_WIDTH);

        if(cursor_cell >= VGA_WIDTH * VGA_HEIGHT){
            cursor_cell = INPUT_START_CELL;
        }
        return;
    }

    if(c == '\b'){
        if(cursor_cell > INPUT_START_CELL){
            cursor_cell--;

            vga[cursor_cell] = ((uint16_t)0x0A << 8) | ' ';
        }
        return;
    }
    
    if(c == '\t'){
        for(int i=0; i<4; i++){
            vga_put_char(' ');
        }
        return;
    }
    
    vga[cursor_cell] = ((uint16_t)0x0A << 8) | (uint16_t)c;
    cursor_cell++;

    if(cursor_cell >= VGA_WIDTH * VGA_HEIGHT){
        cursor_cell = INPUT_START_CELL;
    }
}

int keyboard_init(void)
{
    uint8_t config;


    // 초기화 중 IRQ가 발생하지 않도록 첫 번째와 두 번째 PS/2 포트를 잠시 비활성화
    if (!ps2_write_command(0xAD)){
        return 0;
    }

    // 두 번째 포트가 없는 시스템에서도 해당 명령은 무해함
    (void)ps2_write_command(0xA7);
    ps2_flush_output();


    // i8042 컨트롤러 설정 바이트 읽기.
    if (!ps2_write_command(0x20)){
        return 0;
    }

    if (!ps2_read_data(&config)){
        return 0;
    }


    // 초기화 중에는 컨트롤러 IRQ1과 IRQ12를 끔
    config &= (uint8_t)~0x01;
    config &= (uint8_t)~0x02;

    // bit 4 = 첫 번째 PS/2 포트 clock disable
    // 0으로 만들어 clock을 허용
    config &= (uint8_t)~0x10;


    // vbit 6 = 스캔 코드 변환 활성화
    // 키보드의 세트 2 스캔 코드를 컨트롤러가 세트 1로 변환
    config |= 0x40;

    if (!ps2_write_command(0x60)){
        return 0;
    }

    if (!ps2_write_data(config)){
        return 0;
    }

    // 첫 번째 PS/2 포트 활성화
    if (!ps2_write_command(0xAE)){
        return 0;
    }

    ps2_flush_output();


    // 기본 설정 복원
    if (!keyboard_send_command(0xF6)){
        return 0;
    }

    // 키보드 자체 스캔 코드 세트를 세트 2로 설정
    // i8042가 세트 1로 변환
    if (!keyboard_send_command(0xF0)){
        return 0;
    }

    if (!keyboard_send_command(0x02)){
        return 0;
    }

    // 키보드 스캔 시작
    if (!keyboard_send_command(0xF4)){
        return 0;
    }

    
    //초기화가 끝났으므로 i8042의 IRQ1을 활성화
    if (!ps2_write_command(0x20)){
        return 0;
    }

    if (!ps2_read_data(&config)){
        return 0;
    }

    config |= 0x01;
    config &= (uint8_t)~0x02;
    config &= (uint8_t)~0x10;
    config |= 0x40;

    if (!ps2_write_command(0x60)){
        return 0;
    }

    if (!ps2_write_data(config)){
        return 0;
    }

    return 1;
}

void keyboard_handler(void){
    uint8_t scancode = inb(PS2_DATA_PORT);

    interrupt_count++;

    status_show_hex(scancode);
    status_show_counter(interrupt_count);

    // 확장 scancode는 prefix
    // 화살표 같은 거는 아직 출력 x
    if(scancode == 0xE0 || scancode == 0xE1){
        extended_scancode = 1;
        return;
    }
    if(extended_scancode){
        extended_scancode = 0;
        return;
    }

    // bit 7이 켜진 코드는 key release
    if(scancode & 0x80){
        return;
    }
    if(scancode < sizeof(scancode_to_ascii)){
        char c = scancode_to_ascii[scancode];
        if(c != 0){
            vga_put_char(c);
        }
    }

    // EOI는 boot/isr.S에서 전송
}
