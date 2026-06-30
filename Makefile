# ===================================
#          AIOS Makefile
# ===================================

# 컴파일러 및 링커 프로그램
CC = gcc
LD = ld

# 컴파일 옵션
CFLAGS = -m64 -nostdlib -fno-builtin -ffreestanding -O2 -Wall
# -m64: 64 bit 기계어 생성
# -nostdlib: printf 같은 호스트 os 표준 라이브러리 차단
# -fno-builtin: GCC가 함수를 자기 마음대로 최적화하는 것 금지
# --ffreestanding: main OS 없이 하드웨어 위에 직접 베어메탈로 지정
# -O2: 기계어 최적화 Optimization Level 2
# -Wall: Warning all -> Warning을 모두 보여주도록

# 임시 build 및 iso 출력 디렉토리 지정
BUILD_DIR = build
ISO_DIR = iso

# 최종 결과물 이름
TARGET = kernel.bin
ISO_TARGET = aios.iso

# build rules
# 기본 명령: IOS 디스크 이미지 생성
all: $(ISO_TARGET)

# 어셈블리(boot.S) 컴파일 rule
$(BUILD_DIR)/boot.o: boot/boot.S
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c boot/boot.S -o $(BUILD_DIR)/boot.o

# C언어 커널(kernel.c) 컴파일 rule
$(BUILD_DIR)/kernel.o: kernel/kernel.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/kernel.c -o $(BUILD_DIR)/kernel.o

# 링커(linker.ld)를 통해서 하나의 kernel binary로 통합 rule
$(BUILD_DIR)/$(TARGET): $(BUILD_DIR)/boot.o $(BUILD_DIR)/kernel.o
	$(LD) -m elf_x86_64 -T boot/linker.ld $(BUILD_DIR)/boot.o $(BUILD_DIR)/kernel.o -o $(BUILD_DIR)/$(TARGET)

# GRUB 설정 파일 자동 생성 및 IOS 디스크 굽기 rule
$(ISO_TARGET): $(BUILD_DIR)/$(TARGET)
	@mkdir -p $(ISO_DIR)/boot/grub
	@cp $(BUILD_DIR)/$(TARGET) $(ISO_DIR)/boot/
	@echo 'menuentry "AIOS (Artificial Intelligence OS)" {' > $(ISO_DIR)/boot/grub/grub.cfg
	@echo '    multiboot /boot/kernel.bin' >> $(ISO_DIR)/boot/grub/grub.cfg
	@echo '    boot' >> $(ISO_DIR)/boot/grub/grub.cfg
	@echo '}' >> $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO_TARGET) $(ISO_DIR)

# 실행 rule: QEMU 가상머신 띄우고 CD-ROM에 ISO 장착
run: $(ISO_TARGET)
	qemu-system-x86_64 -cdrom $(ISO_TARGET) -vga std -m 1G -display vnc=:1

# 빌드 찌꺼기 파일들 싹 청소하기
clean:
	rm -rf $(BUILD_DIR) $(ISO_DIR) $(ISO_TARGET)