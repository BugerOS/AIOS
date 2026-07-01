# AIOS (Artificial Intelligence OS)

AIOS는 **CPU 기반 온디바이스 AI 연산(추론 및 학습) 효율 극대화**를 목적으로 설계된 x86_64 아키텍처 기반의 독자 마이크로 커널(Microkernel) 프로젝트입니다. 

기존 범용 운영체제(Linux, Windows 등)의 무거운 태스크 스케줄러 오버헤드와 가상 파일 시스템(VFS) 레이어를 과감히 제거하여, CPU 하드웨어가 인공신경망의 순방향 추론(Inference) 및 역방향 전파 학습(Backpropagation/Training) 행렬 연산에만 100% 집중할 수 있는 베어메탈(Bare-metal) 실행 환경을 제공합니다.

---

## 🚀 Key Architectural Specifications

### 1. Dedicated Compute Cores (인터럽트 격리 연산 코어)
* 멀티코어(SMP) 환경에서 특정 CPU 코어들의 하드웨어 인터럽트(IDT) 스케줄링을 완전히 차단 및 격리합니다.
* 0번 코어가 기본적인 I/O를 전담하는 동안, 격리된 연산 코어들은 컨텍스트 스위칭(Context Switching) 비용 `0` 상태로 오직 순방향/역방향 텐서 연산만 무한 루프로 수행하는 '순수 연산 노예 코어'로 가동됩니다.

### 2. Zero-Overhead Training Pipeline & Identity Mapping
* 가상 메모리 주소 번역 오버헤드(TLB Miss)를 제거하기 위해, 모델의 가중치(Weights), 그라디언트(Gradients), 옵티마이저 상태(Optimizer States)를 물리 메모리에 1:1로 고정 매핑(Identity Mapping)합니다.
* 페이지 폴트(`Page Fault`)나 메모리 스와핑 없이 대역폭을 최대로 끌어당겨 CPU 전용 베어메탈 학습을 가속합니다.

### 3. Tensor Arena (고정형 연속 메모리 풀)
* 학습 및 추론 과정에서 빈번하게 일어나는 텐서 데이터의 파편화를 방지하기 위해, 일반적인 동적 할당(`malloc`) 구조를 지양합니다.
* 커널 내부에 텐서 연산 전용의 거대한 연속 물리 메모리 블록인 **Tensor Arena**를 설계하여 메모리 할당/해제 오버헤드를 원천 차단합니다.

---

## 🛠️ Development & Environment Status

* **Target Hardware:** x86_64 (Intel/AMD) Bare-metal
* **Emulation Environment:** QEMU Virtual Machine (`-m 1G`, `-vga std`, `-display vnc=:1`)
* **Bootloader Spec:** Multiboot Specification compliant (GRUB v2.12 패키징)
* **Memory Management Initialization:** 4단계 페이징(PML4, PDPT, PDT) 빌드 및 2MB 대형 페이지(Huge Page)를 이용한 물리 메모리 1GB 하이패스 매핑 완료.
* **Kernel Entry Setup:** GDT(Global Descriptor Table) 수립 및 `ljmp` 파이프라인 플러시를 통한 64비트 롱 모드 진입 및 C 커널 본체(`kernel_main`) 바인딩 완료.

---

## 🗺️ Project Roadmap

- [x] **Phase 1: Bootstrapping** - Multiboot Header 수립 및 32비트 보호 모드 진입
  - IA-32e 4단계 페이징 구조 설계 및 제어 레지스터(`CR0`, `CR3`, `CR4.PAE`) 갱신
  - GDT(Global Descriptor Table) 수립 및 `ljmp` 파이프라인 플러시를 통한 순도 100% 64비트 롱 모드 가동
  - 64비트 C언어 커널 본체(`kernel_main`) 바인딩 완료
- [x] **Phase 2: Hardware Interrupt & I/O Subsystem** (Next Step)
  - IDT(Interrupt Descriptor Table) 및 PIC(인터럽트 컨트롤러) 마스터
  - 키보드 드라이버 디바이스 핸들링 및 대화형 쉘(Shell) 터미널 구축
- [x] **Phase 3: Tensor Arena Allocator**
  - 파편화 없는 텐서 할당을 위한 연속 물리 메모리 풀 관리 엔진 구축
- [ ] **Phase 4: AVX/AMX Acceleration & Bare-metal Matrix Engine**
  - CPU 내부의 고성능 SIMD 레지스터(AVX-512 등)를 직접 통제하는 순방향/역방향 연산 커널 루틴 적재

---

## 📦 How to Build and Run

프로젝트 루트 디렉토리에서 GNU Make 스크립트를 이용해 빌드 및 VNC 디스플레이 에뮬레이션을 실행합니다.

```bash
# 기존 빌드 리소스 청소
$ make clean

# boot.S, kernel.c 빌드 및 ISO 디스크 이미지 굽기 후 QEMU 실행
$ make run