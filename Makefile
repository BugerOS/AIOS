# ===================================
#          AIOS Makefile
# ===================================

# 컴파일러 및 링커 프로그램
CC = gcc
LD = ld

# 컴파일 옵션
CFLAGS = -m64 -nostdlib -fno-builtin -ffreestanding \
	-O2 -Wall -fno-stack-protector -fno-pie -fno-pic \
	-mno-red-zone -fno-unwind-tables -fno-asynchronous-unwind-tables \
	-fno-tree-loop-distribute-patterns -mgeneral-regs-only \
	-Wextra
# -m64: 64 bit 기계어 생성
# -nostdlib: printf 같은 호스트 os 표준 라이브러리 차단
# -fno-builtin: GCC가 함수를 자기 마음대로 최적화하는 것 금지
# --ffreestanding: main OS 없이 하드웨어 위에 직접 베어메탈로 지정
# -O2: 기계어 최적화 Optimization Level 2
# -Wall: Warning all -> Warning을 모두 보여주도록

# ELF64 커널 링커 옵션
LDFLAGS = -m elf_x86_64 -T boot/linker.ld

# 임시 build 및 iso 출력 디렉토리 지정
BUILD_DIR = build
ISO_DIR = iso

# 최종 결과물 이름
TARGET = kernel.bin
ISO_TARGET = aios.iso

OBJECTS = $(BUILD_DIR)/boot.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/keyboard.o \
	$(BUILD_DIR)/idt.o $(BUILD_DIR)/pic.o $(BUILD_DIR)/isr.o $(BUILD_DIR)/arena.o

.PHONY: all run clean

# build rules
# 기본 명령: IOS 디스크 이미지 생성
all: $(ISO_TARGET)

# 어셈블리(boot.S) 컴파일 rule
$(BUILD_DIR)/boot.o: boot/boot.S
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c boot/boot.S -o $(BUILD_DIR)/boot.o

# C언어 커널(kernel.c) 컴파일 rule
$(BUILD_DIR)/kernel.o: kernel/kernel.c kernel/keyboard.h boot/idt.h boot/pic.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/kernel.c -o $(BUILD_DIR)/kernel.o

# pic를 위한 rule
$(BUILD_DIR)/pic.o: boot/pic.c boot/pic.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c boot/pic.c -o $(BUILD_DIR)/pic.o

# irs를 위한 rule
$(BUILD_DIR)/isr.o: boot/isr.S
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c boot/isr.S -o $(BUILD_DIR)/isr.o

# arena를 위한 rule
$(BUILD_DIR)/arena.o: kernel/arena.c kernel/arena.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/arena.c -o $(BUILD_DIR)/arena.o


# keyboard를 위한 rule
$(BUILD_DIR)/keyboard.o: kernel/keyboard.c kernel/keyboard.h boot/pic.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/keyboard.c -o $(BUILD_DIR)/keyboard.o

# 링커(linker.ld)를 통해서 하나의 kernel binary로 통합 rule
# 링킹 타겟 룰도 idt.o와 pic.o를 결합하도록
$(BUILD_DIR)/$(TARGET): $(OBJECTS) boot/linker.ld
	$(LD) $(LDFLAGS) $(OBJECTS) -o $(BUILD_DIR)/$(TARGET)

# GRUB 설정 파일 자동 생성 및 IOS 디스크 굽기 rule
$(ISO_TARGET): $(BUILD_DIR)/$(TARGET)
	@mkdir -p $(ISO_DIR)/boot/grub
	@cp $(BUILD_DIR)/$(TARGET) $(ISO_DIR)/boot/
	@echo 'menuentry "AIOS (Artificial Intelligence OS)" {' > $(ISO_DIR)/boot/grub/grub.cfg
	@echo '    multiboot /boot/kernel.bin' >> $(ISO_DIR)/boot/grub/grub.cfg
	@echo '    boot' >> $(ISO_DIR)/boot/grub/grub.cfg
	@echo '}' >> $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO_TARGET) $(ISO_DIR)

# interrupt 관련 추가
$(BUILD_DIR)/idt.o: boot/idt.c boot/idt.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c boot/idt.c -o $(BUILD_DIR)/idt.o

# 실행 rule: QEMU 가상머신 띄우고 CD-ROM에 ISO 장착
run: $(ISO_TARGET)
	qemu-system-x86_64 -machine pc -cdrom $(ISO_TARGET) \
		-vga std -m 1G -display vnc=:1 -k en-us \
		-monitor stdio -no-reboot -no-shutdown
# 빌드 찌꺼기 파일들 싹 청소하기
clean:
	rm -rf $(BUILD_DIR) $(ISO_DIR) $(ISO_TARGET)