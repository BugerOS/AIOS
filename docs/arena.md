# Tensor Arena Memory Allocator

## 1. Overview

현재 AIOS는 대규모 텐서 연산 시 발생할 수 있는 메모리 파편화(fragmentation)와 할당 오버헤드를 해결하기 위해서 '물리 메모리 독점 가산 할당기'를 구현합니다. 

현재 테스트 환경에서 아레나 엔진은 부팅 시 identity map으로 확보한 청정 물리 RAM 영역 중 일부를 선점하여 독점 경기장으로 사용합니다. 

현재 allocator는 다음과 같은 기능을 구현합니다. 

- 64 바이트 단위의 하드웨어 경계 정렬
- O(1) 속도의 초고속 포인터 전진 할당
- 메모리 전량 초기화 시 클럭 소모 최소화
- Out of Memory 방지

아직 구현하지 않은 기능은 다음과 같습니다. 

- memory free: 블록 단위 개별 메모리 해제
- garbage collection: 아레나 내부 동적 조각 모음
- swap file: 가상 메모리 페이지 스왑

## 2. Allocation Flow

    matrix_create() 또는 arena_alloc() 호출
        ↓
    요청 크기 수신 (Byte 단위)
        ↓
    64바이트 배수로 올림 팩킹 연산 (Bit Masking)
        ↓
    [Out Of Memory 검문] (현재 오프셋 + 정렬된 크기) > 아레나 최대 제한폭 (900MB)
        ├─ True  : NULL 반환 (Out Of Memeory Guard 작동)
        └─ False : 할당 파이프라인 계속
        ↓
    현재 아레나 베이스 주소 + 오프셋 계산 → 할당할 물리 주소 확정
        ↓
    아레나 오프셋에 할당 크기만큼 단순 가산
        ↓
    확정된 물리 메모리 주소 반환

arena allocator는 빈 메모리 조각을 찾기 위해서 linkded list를 순회하거나 address table을 탐색하지 않습니다. 오직 포인터를 앞으로 밀며 전진하여 하드웨어 레벨에서 가장 빠른 속도를 보장합니다. 

※ trade off: 공간 효율성을 포기하고, 시간 효율성을 취합니다. 

## 3. HW Composition & Specification

### 3-1. 물리 메모리 배분

부팅 시 페이지 테이블이 일대일 identity map으로 뚫어놓은 전체 물리 메모리를 아래에 맞게 배분합니다. 

물리 주소 범위 | 크기 | 용도 |
|---|---|---|
| 0x00000000 ~ 0x000FFFFF | 1 MB | BIOS 및 VGA 비디오 버퍼 (0xB8000) 시스템 구역 |
| 0x00100000 ~ 0x001FFFFF | 1 MB | kernel.bin 기계어 및 IDT/GDT 구역 |
| 0x00200000 ~ 0x00207FFF | 32 KB | 커널 전용 독립 스택 구역 |
| 0x04000000 ~ 0x3C000000 | 900 MB | Tensor Arena 독점 영토 (최대 제한폭) |
| 0x3C000000 ~ 0x40000000 | 약 60 MB | 다중 코어 및 하드웨어 MMIO 칩셋 예비 버퍼 구역 |

※ arena가 1GB 한계선을 넘어서면 page fault가 발생하기 때문에 최대 할당 크기를 900MB로 제한합니다. 

### 3-2. AVX-512 하드웨어 가속 정렬

Intel Xeon 및 AMD Ryzen 제품군에 탑재된 AVX-512 가속 하드웨어 유닛은 한 클럭에 512 비트 데이터를 동시에 계산합니다. 

512 비트는 바이트로 환산하면 정확하게 64 바이트입니다. 

CPU 배선 유닛이 RAM에서 데이터를 한 번에 긁어오려면 물리 주소가 64의 배수의 경계에 있어야 합니다. 

주소가 어긋나 있으면 data load가 거부되거나 시스템이 죽어버릴 수 있어서 비트 연산 조항을 통과시킵니다. 

    size = (size + 63) & ~((size_t)63);

=> 홀수 바이트라도 하드웨어가 요구하는 최적의 배수로 자동 반올림됩니다. 

## 4. Main Interface

| 함수명 | 역할 | 속도 |
|---|---|---|
| arena_init() | 아레나 시작 주소 세팅 및 오프셋 초기화 | O(1) | 
| arena_alloc(size) | 64바이트 정렬 후 포인터를 전진시켜 주소 반환 | O(1) |
| arena_reset() | 오프셋을 0으로 꺾어 아레나 공간을 통째로 비움 | 0 클럭 | 

## 5. 디버깅 및 런타임 검증 결과

vga_write_hex 포맷터를 통해서 실전 QEMU 런타임 주소를 확인한 결과 무결한 것으로 보입니다. 

- Tensor 1 address: 0x0000000004000000(arena 기점 정위치)
- Tensor 2 address: 0x00000000040F4280(첫 할당 크기가 홀수임에도 하위 비트가 0x80으로 정렬됨)
- Out of Memory Guard: 950MB 과할당 요청 시 시스템이 멈추지 않고 PASS: out of memory guard Locked 정상 출력

## 6. 파일 별 역할

| 파일 | 역할 |
|---|---|
| kernel/arena.h | 아레나 엔진 가동 및 64바이트 가속 정렬 함수 인터페이스 명세 |
| kernel/arena.c | Bump Allocator 포인터 가산 및 OOM 한계선 상한 차단막 구현 |
| kernel/kernel.c | 아레나 엔진 초기화 및 텐서 1, 2 실전 런타임 주소 디버깅 테스트 |
| Makefile | 자동 변수 제거 후 arena.o 절대 경로 컴파일 및 링킹 규칙 반영 |