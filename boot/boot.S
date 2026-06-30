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
.global start

start:
    // hw interrupt 차단
    cli

    // C언어를 위한 임시 stack 세팅
    movl $KERNEL_STACK, %esp

    // 4단계 paging 하드웨어 세팅 함수 호출
    call set_up_paging

    //MSR(특수 register)을 조작해서 64 bit 롱 모드(EFER.LME) 활성화
    movl $0xC0000080, %ecx
    rdmsr // read msr(model specific register)
    orl $(1 << 8), %eax
    wrmsr // msr 값을 덮어씀

    // CR3에 page table 등록
    movl $PAGE_TABLE_BASE, %eax
    movl %eax, %cr3

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
    call kernel_main

// kernel이 종료되면 cpu를 정지(halt)
_halt:
    hlt
    jmp _halt

.code32
// 페이징 세팅 함수
// 2MB 대형 page 512개를 물리 메모리 1GB에 하이패스로 매핑
set_up_paging:
    movl $PAGE_TABLE_BASE, %edi
    movl $4096, %ecx
    xorl %eax, %eax
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
    addl $0x200000, %ebx // 다음 물리 주소 경계로 2MB 점프(띄우기)
    addl $8, %edi // 64 bit entry 주소 -> 8 byte 씩 이동
    loop .set_pdt_loop // counter인 %ecx를 1 감소하고, 0이 아나라면 loop 계속, 0이면 loop 종료

    // 제어 레지스터 CR3에 내가 만든 최상위 테이블 주소(0X9000)를 등록
    // movl $PAGE_TABLE_BASE, %eax
    // movl %eax, %cr3

    // CR4 레지스테에서 x64 필수 조건인 PAE 비트(5번째) 활성화
    movl %cr4, %eax
    orl $(1 << 5), %eax
    movl %eax, %cr4
    
    ret // 함수 호출이 끝나서 호출했던 부분으로 돌아감


// %esp: stack pointer
// %eax accumulator
// $ecx: counter

// CR0, CR3, CR4는 하드웨어 스위치 제어판

// 64 bit 진입을 위한 하드웨어 최소 표준 제어판 (GDT)
.align 4
gdt_start:
    .quad 0x0000000000000000 // null descriptor -> 없으면 안됨
    .quad 0x00209A0000000000 // 64 bit code segment descriptor (0x08)
gdt_end:

gdt_ptr:
    .word gdt_end - gdt_start -1
    .long gdt_start