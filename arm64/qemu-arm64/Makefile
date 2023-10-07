CC=aarch64-none-elf-gcc
LD=aarch64-none-elf-ld
OBJCOPY=aarch64-none-elf-objcopy

all: clean build

build: boot.o qemu_arm64.o
	$(LD) -T linker.ld boot.o qemu_arm64.o -o qemu_arm64_test.elf
	$(OBJCOPY) -O binary qemu_arm64_test.elf qemu_arm64_test.bin

boot.o: boot.s
	$(CC) -c $^ -o $@

qemu_arm64.o: qemu_arm64.c
	$(CC) -c $^ -o $@

clean:
	rm -rf boot.o
	rm -rf qemu_arm64.o
	rm -rf qemu_arm64_test.elf

run:
#on Aarch64 device
	qemu-system-aarch64 -M virt -cpu host -enable-kvm -nographic -kernel qemu_arm64_test.elf
#other platform we need to emulation
	qemu-system-aarch64 -M virt -cpu cortex-a72 -nographic -kernel qemu_arm64_test.elf
	