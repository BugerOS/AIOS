// boot.S
# define MULTIBOOT_MAGIC 0x1BADB002 // GRUB가 이 커널을 알아차리기 위한 magic value
# define MULTIBOOT_FLAGS 0x00000003 // page 정렬 및 mem 정보 요청 flag
# define MULTIBOOT_CHECKSUM -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

# define PAGE_TABLE_BASE 0X9000 // page table 구축 위한 safety region
# define KERNEL_STACK 0X9F000 // C언어를 위한 초기 스택 포인터 위치

.code32 // GRUB가 처음 AIOS를 실행했을 때는 32bit privileged mode일 것

.section .multiboot // 아래의 데이터들을 .multiboot section에 따로 보관
.align 4 // 4 byte 단위로 정렬
    .long MULTIBOOT_MAGIC
    .long MULTIBOOT_FLAGS
    .long MULTIBOOT_CHECKSUM

.section .text // 여기부터는 진짜 기계어 명령들
.code32

.global start
.extern kernel_main

start:
    // hw interrupt 차단
    cli

    // 문자열 명령의 진행 방향은 정방향
    cld

    // C언어를 위한 임시 stack 세팅
    movl $stack_top, %esp

    // 4단계 paging 하드웨어 세팅 함수 호출
    call set_up_paging

    // CR4 레지스터에서 PAE 비트(5번재)를 선제 활성화
    movl %cr4, %eax
    orl $(1 << 5), %eax
    movl %eax, %cr4

    // 최상위 페이지 디렉토리 주소를 cr3에 등록
    movl $PAGE_TABLE_BASE, %eax
    movl %eax, %cr3

    //MSR(특수 register)을 조작해서 64 bit 롱 모드(EFER.LME) 활성화
    movl $0xC0000080, %ecx
    rdmsr // read msr(model specific register)
    orl $(1 << 8), %eax
    wrmsr // msr 값을 덮어씀

    // 임시 GDT 세팅
    lgdt gdt_ptr

    // CR0 레지스터의 paging bit 킴 -> 64 bit 가동
    movl %cr0, %eax
    orl $(1 <<31), %eax
    movl %eax, %cr0

    // C언어로 작성될 kernel의 본체를 호출
    // .global kernel_main이 들어올 자리
    // call kernel_main
    ljmp $0x08, $start_64

.code64
start_64:
    // 여기부터는 64bit long mode -> C언어로 된 kernel 안전하게 호출 가능
    // 데이터 세그먼트 selector 0x10 설정
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss

    // fs와 gs는 현재 사용 x -> null selector
    xorw %ax, %ax
    movw %ax, %fs
    movw %ax, %gs
    // 64비트 rsp를 실제 커널 스택 끝으로 설정
    // rsp: register stack pointer
    leaq stack_top(%rip), %rsp

    // c abi를 위해서 스택을 16 바이트로 정렬
    andq $-16, %rsp

    // x87 및 SSE 사용 준비
    movq %cr0, %rax
    andq $-13, %rax
    orq $0x02, %rax
    movq %rax, %cr0

    // os가 sse 상태와 예외를 지원함을 cpu에 알림
    movq %cr4, %rax
    orq $0x600, %rax
    movq %rax, %cr4
    
    // x87 fpu 초기화
    fninit

    // 초기 frame pointer는 0
    // xorq %rbp, %rbp

    call kernel_main

// kerenel main은 정상적응로 return x
// -> return하게 되면 cpu 정지
.kernel_returned:
    cli
    hlt
    jmp .kernel_returned

.code32
// 페이징 세팅 함수
// 2MB 대형 page 512개를 물리 메모리 1GB에 하이패스로 매핑
set_up_paging:
    movl $PAGE_TABLE_BASE, %edi
    movl $4096, %ecx
    xorl %eax, %eax
    cld

    // xorl: 32비트 크기로 XOR(배타적 논리합) 비트 연산을 수행
    rep stosl // 0x9000 ~ 0xD000 영역을 0으로 초기화

    // PML4T(0x900)가 다음 단계인 PDPT(0XA000)를 가리키도록 함
    // present + RW = 0x3
    movl $((PAGE_TABLE_BASE + 0X1000) | 3), (PAGE_TABLE_BASE)
    
    // PDPT(0xA00)가 다음 단계인 PDT(0XB000)를 가리키도록 함
    // present + RW = 0x3
    movl $((PAGE_TABLE_BASE + 0X2000) | 3), (PAGE_TABLE_BASE + 0X1000)

    // PDT(0XB000) 내부를 로프로 돌면서 2MB 대형 page flag(0x83)을 사용해서 1GB 영역 통째로 매핑
    // 0x83은 1000 0011 -> 비트 하나하나가 cpu 하드웨어 on/off 스위치 역할
    movl $(PAGE_TABLE_BASE + 0x2000), %edi
    movl $0x83, %ebx // 0번 물리 주소부터 시작해서 present(1) + RW(2) + HugePage(0x80)
    movl $ 512, %ecx

.set_pdt_loop:
    movl %ebx, (%edi) //(%edi)는 pointer라고 생각
    movl $0, 4(%edi) //페이지 엔트리 상위 32비트는 0
    addl $0x200000, %ebx // 다음 물리 주소 경계로 2MB 점프(띄우기)
    addl $8, %edi // 64 bit entry 주소 -> 8 byte 씩 이동
    loop .set_pdt_loop // counter인 %ecx를 1 감소하고, 0이 아나라면 loop 계속, 0이면 loop 종료

    // 제어 레지스터 CR3에 내가 만든 최상위 테이블 주소(0X9000)를 등록
    // movl $PAGE_TABLE_BASE, %eax
    // movl %eax, %cr3

    // CR4 레지스테에서 x64 필수 조건인 PAE 비트(5번째) 활성화
    // movl %cr4, %eax
    // orl $(1 << 5), %eax
    // movl %eax, %cr4
    
    ret // 함수 호출이 끝나서 호출했던 부분으로 돌아감


// %esp: stack pointer
// %eax accumulator
// $ecx: counter

// CR0, CR3, CR4는 하드웨어 스위치 제어판

// 64 bit 진입을 위한 하드웨어 최소 표준 제어판 (GDT)
.align 8
gdt_start:
    .quad 0x0000000000000000 // null descriptor -> 없으면 안됨
    .quad 0x00AF9A000000FFFF
    .quad 0x00CF92000000FFFF
gdt_end:

gdt_ptr:
    // gdt 크기 - 1
    .word gdt_end - gdt_start -1

    //32비트에서 lgdt를 실행 -> 베이스 주소를 32비트로 기록
    .long gdt_start

// 실제 커널 스택 공간
// 32kb를 확보하고 스택은 밑으로 자람
.section .bss
.align 16

stakc_bottom:
    .skip 32768
stack_top:

.section .note.GNU-stack,"",@progbits