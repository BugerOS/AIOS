// 인터럽트 로우 레벨 어셈블리 래퍼 구현
// cpu가 IRQ1을 받았을 때 최초로 들어오는 어셈블리 코드
// boot/isr.S

.section .text
.code64

.global isr_keyboard_wrapper
.global isr_default_wrapper

.extern keyboard_handler

// 알 수 없는 인터럽트 또는 예외 시 사용
// 일단은 cpu 정지시키는 걸로
isr_default_wrapper:
    cli

.default_halt:
    hlt
    jmp .default_halt

// 키보드 인터럽트(33번) -> cpu가 최초로 진입하는 지점
isr_keyboard_wrapper:
    // 현재 연산 중이던 cpu 레지스터 상태를 스택에 안전하게 대피(context save)
    // 실행 시 레지스터들은 dirty해짐
    // -> 연산만 하던 core의 레지스터 값이 틀어지면 AI 가중치 깨지거나 kernel die
    // -> 현재 값을 stack mem에 저장
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rbp
    pushq %rsi
    pushq %rdi

    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    
    // x86_64 16 byte 스택 정렬 패딩 강제 주입
    // 16의 배수 타이밍 맞추기
    // pushq %rbp

    // 현재 스택 주소를 rbx에 보관
    movq %rsp, %rbx

    // c언어 함수 호출 전 스택을 16 바이트 경계에 맞춤
    andq $-16, %rsp

    // direction flag를 0으로
    // c 컴파일러는 문자열 명령이 정방향으로 동작한다고 가정
    cld // clear direction flag

    // driver bridge: 진짜 driver 로직으로 jmp
    // 레지스터가 완변히 보호된 샌드박스 상태에서 키보드 스캔 및 화면 출력 핸들러
    // 참고: C언어로 작성됨
    call keyboard_handler

    // 16바이트 정렬 전의 원래 스택 위치로 돌아감
    movq %rbx, %rsp

    // 키보드 irq1은 master pic 소속
    // pic에게 eoi를 보내지 않으면 다음 키보드 인터럽트 전달 x
    // 이전 인터럽트가 아직 핸들링 중이라고 보기 때문
    movb $0x20, %al
    outb %al, $0x20

    // 주입했던 8byte 가짜 패딩 수동 수거
    //popq %rbp

    // context restore: 복구
    // stakc mem에 저장했던 연산 register 값들을 역순으로 복원
    // LIFO: Last In First Out
    // popq %r11
    // popq %r10
    // popq %r9
    // popq %r8
    // popq %rdi
    // popq %rsi
    // popq %rdx
    // popq %rcx
    // popq %rax

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8

    popq %rdi
    popq %rsi
    popq %rbp
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    
    // Interrupt return: 인터럽트 파이프라인 정리
    // 일반 함수 리턴 명령 ret x
    // iretq(Interrupt Return Quadword)
    // cpu에게 interrupt 전 수행 중이던 RIP와 flag 상태를 원상복구하고 돌아가라는 privileged 기계어
    // rip: 코드 번지
    iretq

// 보안 가이드라인 명시
.section .note.GNU-stack,"",@progbits