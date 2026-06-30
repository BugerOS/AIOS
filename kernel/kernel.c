// kernel.c
// VGA 텍스트 모드의 물리 메모리 주소 (화면 출력 버퍼)
# define VIDEO_MEMORY 0xB8000
# define WIDTH 80
# define HEIGHT 25

void kernel_main(void){

    unsigned short* video_buffer = (unsigned short*)VIDEO_MEMORY;

    // 화면 전체를 빈 화면으로(가로 80 x 세로 25로 공간을 초기화)
    for(int i=0; i< WIDTH * HEIGHT; i++){
        video_buffer[i] = (0x0F << 8) | ' ';
        // << 8 하는 이유: VGA 텍스트 모드 하드웨어는 화면의 글자 한 칸 당 2 byte 소모
        // 하위 1 바이트(0~7비트)는 문자
        // 상위 1 바이트는 색상과 같은 속성
        // 0x0F는 검은 배경에 흰 글씨
    }

    const char* msg = "Welcome to AIOS! 64-bit Kernel Successfully Loaded.";

    // 비디오 메모리에 문자 넣기
    for(int i=0; msg[i]!='\0'; i++){
        video_buffer[i] = (0x0F << 8) | msg[i];
    }

    // kernel이 종료되어 CPU가 대기 상태를 벗어나는 것을 방지
    while(1){
        // CPU가 100% 로드를 먹여 과열되지 않도록 하드웨어 halt 명령
        __asm__ __volatile__("hlt");
    }
}