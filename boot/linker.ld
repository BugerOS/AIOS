/* linker.ld */
/* 링커: 기계어를 mem에 배치  */

ENTRY(start)
/* kernel의 첫 시작점 함수 이름이 start라고 선언 */

SECTIONS{
    /* kenrel을 물리 주속 1MB(0x100000) 자리에 배치 */
    . = 0x100000;
    /* text section: 어셈블리나 c언어 기계어 코드들 위치 */
    .text : ALIGN(4096)
    {
        /* GRUB 부트로더가 커널을 인지하면 멀티부트 헤더가 무조건 맨 앞에 와야 함 */
        KEEP(*(.multiboot))
        *(.text)
        *(.text.*)
    }

    /* 읽기 전용 data section: 문자열 상수 등 */
    /* read only data */
    .rodata : ALIGN(4096)
    {
        *(.rodata)
        *(.rodata.*)
    }

    /* 읽기/쓰기 가능한 전역 변수 data section  */
    .data : ALIGN(4096)
    {
        *(.data)
        *(.data.*)
    }

    /* 초기화 x 전역 변수 및 stack region */
    .bss : ALIGN(4096)
    {
        *(.bss)
        *(.bss.*)
        *(COMMON)
        /* Block Started by Symbol */
    }
    
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}
/* 
    *은 C의 pointer가 아니라 coalescing을 위한 것 
    x86_64 arch에서는 
    32 bit 보호 모드 -> paging disabled 해도 컴퓨터 동작
    64 bit 롱 코드 -> paging이 무조건 enable되어 있어야 함
    => 64 bit를 위해서는 CR3 레지스터가 제자리에 있어야 함
    단, 여기서는 물리 메모리를 통째로 확보하기 위한 것
    즉, CPU를 64비트 모드로 진화시키기 위한 하드웨어 필수 조건
*/