# PS/2 Keyboard Driver

## 1. Overview

현재 AIOS는 i8042 호환 PS/2 키보드 컨트롤러를 지원합니다.

현재 테스트 환경에서 AIOS의 키보드 입력은 QEMU가 제공하는 PS/2 키보드 장치를 통해서 전달됩니다. 

현재 드라이버는 다음과 같은 기능을 구현합니다. 

    - 영문 소문자 입력
    - 숫자 및 기본 기호 입력
    - Enter 처리
    - Backspace 처리
    - Tab 처리
    - 최근 스캔 코드 표시
    - 키보드 IRQ 누적 횟수 표시

아직 구현하지 않은 기능은 다음과 같습니다. 

    - Shift
    - Caps Lock
    - 한글 입력
    - 방향키 및 확장 스캔 코드 처리
    - 키보드 LED 제어
    - 화면 스크롤

※ 한글 입력은 단순한 스캔 코드 변환만으로는 구현할 수 없습니다. 키 상태 처리 이외에도 초성, 중성, 종성을 조합하는 로직이 추가로 필요합니다. 

---

## 2. Input Flow

    키보드 누름
        ↓
    QEMU의 PS/2 키보드 장치
        ↓
    i8042 PS/2 컨트롤러
        ├─ 스캔 코드 데이터: I/O 포트 0x60
        └─ 하드웨어 인터럽트: IRQ1
        ↓
    8259 PIC
        ↓
    IRQ1 + Master PIC 시작 벡터 0x20
        ↓
    IDT 벡터 0x21
        ↓
    isr_keyboard_wrapper
        ↓
    keyboard_handler()
        ↓
    Master PIC에 EOI 전송
        ↓
    iretq로 인터럽트 이전 코드에 복귀

키보드는 완성된 문자를 직접 보내는 것이 아니라 키가 눌리거나 떼어지는 지 여부를 나타내는 스캔 코드를 전달합니다. 

운영체제는 키보드 배열, Shift, CapsLock 등의 상태, 입력 언어를 함께 고려해서 이를 문자로 변환합니다. 

    Why?

    1. 같은 키가 항상 같은 문자를 뜻하지는 않습니다. 
    2. Shift, Ctrl, Alt 자체는 문자가 아니기 때문입니다. 
    3. 키를 누른 것과 뗀 것을 구분해야 하기 때문입니다. 
    4. 언어와 문자 변환을 하드웨어와 분리하기 위함입니다. (영어, 한국어, 일본어 등)

    => 결론적으로 완성된 문자를 직접 보내지 않고 스캔 코드를 전달한 이유는 확장성 때문입니다. 

예를 들어서 Scan Code Set 1 기준으로는 다음과 같은 값이 사용됩니다. 

| 키 | 눌렀을 때 | 뗐을 때 |
|---|---:|---:|
| A | `0x1E` | `0x9E` |
| B | `0x30` | `0xB0` |
| C | `0x2E` | `0xAE` |

※ 키를 눌렀을 때 발생하는 코드를 make code, 키를 뗐을 때 발생하는 코드를 break code라고 합니다. 

---

## 3. HW Composition

### 3-1. i8042 PS/2 컨트롤러

PS/2 컨트롤러는 키보드와 CPU 사이에서 데이터를 중계합니다. 

주요 I/O 포트는 다음과 같습니다. 

| 포트 | 용도 |
|---|---|
| `0x60` | 키보드 데이터 읽기 및 데이터 쓰기 |
| `0x64` | 컨트롤러 상태 읽기 및 명령 쓰기 |

0X64 상태 포트의 주요 비트는 다음과 같습니다. 

| 비트 | 의미 |
|---|---|
| bit 0 | 출력 버퍼에 읽을 데이터가 있음 |
| bit 1 | 입력 버퍼가 사용 중이며 아직 쓰면 안 됨 |

※ 컨트롤러에 명령을 쓰기 전에는 bit 1이 0인지 확인해야 합니다. 

※ 키보드 데이터를 읽기 전에는 bit 0이 1인지 확인해야 합니다. 

---

### 3-2. 8259 PIC

PIC는 하드웨어 IRQ를 CPU의 IDT 벡터로 전달합니다. 

AIOS는 PIC를 다음과 같이 재배치합니다. 

    Master PIC IRQ0~IRQ7   → IDT 0x20~0x27
    Slave PIC IRQ8~IRQ15   → IDT 0x28~0x2F

PS/2 키보드는 IRQ1을 사용하므로 최종 IDT 벡터는 다음과 같습니다. 

    Master PIC 시작 벡터는 0x20 + IRQ1 = IDT 벡터 0x21

PIC 마스크 비트는 다음과 같은 의미를 가집니다. 

    0: IRQ 허용
    1: IRQ 차단

※ 키보드 초기화가 완료되기 전에는 모든 IRQ를 차단하고, 완료 후에는 IRQ1만 허용합니다. 

---

## 4. 초기화 순서

키보드 초기화는 다음의 순서대로 이뤄집니다. 

     CPU 인터럽트 차단
            ↓
         IDT 설치
            ↓
    PIC 재배치 및 모든 IRQ 차단
            ↓
    i8042 PS/2 컨트롤러 초기화
            ↓
     키보드 장치 초기화
            ↓
      PIC에서 IRQ1 허용
            ↓
      CPU 인터럽트 허용

※ 초기화가 끝나기 전에 IRQ1을 열면 설정 도중 발생한 ACK나 남아 있는 데이터가 키보드 입력처럼 처리될 수 있으니 주의가 필요합니다. 

---

## 5. PS/2 컨트롤러 초기화

키보드 초기화 과정은 다음과 같습니다. 

    첫 번째 PS/2 포트 비활성화
                ↓
    두 번째 PS/2 포트 비활성화
                ↓
        기존 출력 버퍼 비우기
                ↓
      컨트롤러 설정 바이트 읽기
                ↓
    초기화 중 IRQ1·IRQ12 비활성화
                ↓
    첫 번째 PS/2 포트 clock 활성화
                ↓
     Scan Code Set 1 변환 활성화
                ↓
      컨트롤러 설정 바이트 쓰기
                ↓
      첫 번째 PS/2 포트 활성화
                ↓
        키보드 기본 설정 복원
                ↓
    키보드 Scan Code Set 2 설정
                ↓
        키보드 스캔 활성화
                ↓
       컨트롤러 IRQ1 활성화

사용하게 된 주요 명령은 다음과 같습니다. 

| 명령 | 의미 |
|---|---|
| `0xAD` | 첫 번째 PS/2 포트 비활성화 |
| `0xA7` | 두 번째 PS/2 포트 비활성화 |
| `0x20` | 컨트롤러 설정 바이트 읽기 |
| `0x60` | 컨트롤러 설정 바이트 쓰기 |
| `0xAE` | 첫 번째 PS/2 포트 활성화 |
| `0xF6` | 키보드 기본 설정 복원 |
| `0xF0` | 키보드 스캔 코드 세트 설정 |
| `0x02` | Scan Code Set 2 선택 |
| `0xF4` | 키보드 스캔 활성화 |

키보드 명령에 대한 대표적인 응답은 다음과 같습니다. 

| 응답 | 의미 |
|---|---|
| `0xFA` | ACK, 명령 정상 수신 |
| `0xFE` | Resend, 명령 재전송 요청 |

---

## 6. Interrupt 처리

CPU가 IDT 벡터 0x21을 통해서 isr_keyboard_wrapper에 진입하면 다음의 작업을 수행합니다. 

       범용 레지스터 저장
               ↓
    C 함수 호출을 위한 스택 정렬
               ↓
      keyboard_handler() 호출
               ↓
        Master PIC에 EOI 전송
               ↓
          레지스터 복구
               ↓
             iretq

### 6-1. Register 저장

인터럽트가 발생하면 원래 실행 중이었던 코드의 레지스터 값을 보존해야 합니다. 

```asm
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
```

복구할 때는 반드시 반대 순서로 pop해야 합니다. 

```asm
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
```

### 6-2. 스택 정렬

System V AMD64 ABI에 맞춰 C언어 함수를 호출하기 전에 스택을 16 바이트 경계로 정렬합니다. 

```asm
movq %rsp, %rbx
andq $-16, %rsp
cld
call keyboard_handler
movq %rbx, %rsp
```

### 6-3. EOI 전송

인터럽트 처리가 끝나면 PIC에 EOI를 보내야 합니다. 

키보드 IRQ1은 master PIC에 속하므로 master에만 EOI를 전송합니다. 

EOI를 보내지 않으면 PIC는 이전 인터럽트가 아직 처리 중이라고 판단하고, 다음 인터럽트를 전달하지 않습니다. 

### 6-4. 인터럽트 복귀

일반적인 함수처럼 ret를 사용하지 않고, iretq를 사용합니다. 

iretq는 CPU가 인터럽트 진입 시 저장한 다음 값을 복구합니다. 

## 7. 스캔 코드 처리

keyboard_handler는 0x60 포트에서 스캔 코드를 읽어옵니다. 

Scan Code Set 1에서는 최상위 비트로 '키를 누르는 것'과 '키를 떼는 것'을 구분할 수 있습니다. 

따라서 키를 눌렀을 때만 문자를 출력하기 위해서 & 연산을 이용합니다. 

```c
if (scancode & 0x80) {
    return;
}
```

일반 키는 스캔 코드를 ASCII 변환 표의 인덱스로 사용합니다. 

이때 확장 키는 0xE0이나 0xE1 prefix를 사용하며, 현재 구현에서는 방향키와 같은 확장키(extended key)를 무시합니다. 

## 8. VGA 문자 출력

VGA 텍스트 모드 메모리는 물리 주소 0xB8000에 있습니다. 

이때 화면 한 칸은 2 바이트로, 상위 1바이트는 색상 속성을 나타내며 하위 1바이트는 문자 코드를 나타냅니다. 

따라서 VGA 메모리는 uint16_t로 접근합니다. 

## 9. 특수 키 처리

### 9-1. Enter

현재 줄의 남은 칸을 건너뛰어서 다음 줄 첫 칸으로 이동합니다. 

```c
cursor_cell +=
    VGA_WIDTH - (cursor_cell % VGA_WIDTH);
```

### 9-2. Backspace

커서를 한 칸 뒤로 이동시킨 뒤, 공백으로 덮어씁니다. 

```c
cursor_cell--;

vga[cursor_cell] =
    ((uint16_t)0x0A << 8) | ' ';
```

### 9-3. Tab

공백 네칸으로 처리합니다. 

```c
for (int i = 0; i < 4; i++) {
    vga_put_char(' ');
}
```

---

## 10. 디버깅 상태 표시

화면 첫 번째 줄에 IO_STAT과 INT_IN을 표시합니다. 

### IO_STAT

최근 수신한 스캔 코드를 16진수로 표시합니다. 

    ex )
    
    A 누름 -> 1E
    A 뗌   -> 9E

### INT_IN

지금까지 발생한 키보드 IRQ 횟수를 표시합니다. 

키를 한 번 눌렀따가 떼면 두 번 증가합니다. 

---

## 11. QEMU 실행 및 테스트

    qemu-system-x86_64 -machine pc -cdrom aios.iso -vga std -m 1G -display vnc=:1 -k en-us -monitor stdio -no-reboot -no-shutdown

## 12. 현재 파일 별 역할

| 파일 | 역할 |
|---|---|
| boot/boot.S | 32비트 부팅, 페이지 테이블 구성, 64비트 Long Mode 진입 |
| boot/idt.c | IDT 생성 및 키보드 벡터 0x21 연결 |
| boot/isr.S | IRQ1 최초 진입, 레지스터 저장, EOI, iretq |
| boot/pic.c | 8259 PIC 재배치 및 IRQ 마스크 제어 |
| kernel/keyboard.c | PS/2 초기화, 스캔 코드 해석, 문자 출력 |
| kernel/kernel.c | 전체 초기화 순서와 커널 대기 루프 |
| boot/linker.ld | 커널 섹션의 메모리 배치 |
| Makefile | 컴파일, 링크, ISO 생성 및 QEMU 실행 |

---

## 13. 향후 계획

    1. Shift 및 CapsLock 키 상태 추적
    2. 방향키나 확장 스캔 코드 처리
    3. VGA 화면 스크롤
    4. 키보드 입력 버퍼 구현
    5. 한글 자모 조합
    6. PS/2 외의 키보드 처리