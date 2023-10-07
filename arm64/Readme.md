## Install KVM on Raspberry Pi 4 (Debian 0S)
`sudo apt install qemu-system-arm`

## For cross compilation 
- install toolchain `aarch64-none-elf-*` from linaro or from os package manager (like apt,rpm) if available

## To run qemu-arm64
### qemu-arm64 is a baremetal program runs inside qemu enabling kvm or can be run on x86 without enable-kvm option as mentioned in Makefile run command and below :
- to build: `make`
- #on Aarch64 device 
	`qemu-system-aarch64 -M virt -cpu host -enable-kvm -nographic -kernel qemu_arm64_test.elf`
- #other platform we need to emulation
	`qemu-system-aarch64 -M virt -cpu cortex-a72 -nographic -kernel qemu_arm64_test.elf`
- sample output
```
Hello what's up!
```