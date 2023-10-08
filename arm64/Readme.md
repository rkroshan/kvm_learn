## Install KVM on Raspberry Pi 4 (Debian 0S)
`sudo apt install qemu-system-arm`

## For cross compilation qemu-arm64 example
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

## For cross compilation kvm_code_bin example
- option 1 : build on target
- rename `Makefile-ontaget-arm64` to `Makefile` and build it on target cmd: `make`

- option 2 : cross-compilation for target (build for raspberrypi 4)
- we will build toolchain via crosstool-ng
- ref: https://ilyas-hamadouche.medium.com/creating-a-cross-platform-toolchain-for-raspberry-pi-4-5c626d908b9d
- steps:
	- `mkdir -p ~/crosstool`
	- `wget http://crosstool-ng.org/download/crosstool-ng/crosstool-ng-1.26.0.tar.xz`
	- `tar -xvf crosstool-ng-1.26.0.tar.xz`
	- `mkdir rasptool`
	- `cd crosstool-ng-1.26.0`
	- `sudo apt-get install automake bison chrpath flex g++ git gperf gawk help2man libexpat1-dev libncurses5-dev libsdl1.2-dev libtool libtool-bin libtool-doc python-is-python3 python3-dev python-dev-is-python3`
	- `./configure --prefix=~/crosstool/rasptool`
	- `make`
	- `make install`
	- `echo "export PATH=$PATH:~/crosstool/rasptool/bin" >> ~/.bashrc`
	- `source ~/.bashrc`
	- `cd ..`
	- `mkdir staging && cd staging`
	- `ct-ng aarch64-rpi4-linux-gnu`
	- Note: one error i faced was glibc version build for cross-compilation is not same as present on my raspberry pi, `/lib/aarch64-linux-gnu/libc.so.6 -> libc-2.31.so`, so I need to change in .config file (created in staging dir) and set `CT_GLIBC_V_2_31=y` and unset `CT_GLIBC_V_2_34`
	- `ct-ng build`
	- `echo "export PATH=$PATH:~/x-tools/aarch64-rpi4-linux-gnu/bin" >> ~/.bashrc`
	- Now , go to kvm_code_bin directory and build the binaries cmd: `make`
	- trasfer the image via scp, tftp etc
	
## To run kvm_code_bin
### kvm_code_bin runs a vm which executes the code present in qemu_aarch64_test.bin, traps the mmio write to uart addr and print it on stdout

- run: `./kvm_code_bin`
- Note: if permission denied run via sudo
- sample output
```
read size: 176
Retrieving physical CPU information
Initializing VCPU
Setting program counter to entry address 0x00000000
KVM start run
KVM_EXIT_MMIO
72 : H 
KVM start run
KVM_EXIT_MMIO
101 : e 
KVM start run
KVM_EXIT_MMIO
108 : l 
KVM start run
KVM_EXIT_MMIO
108 : l 
KVM start run
KVM_EXIT_MMIO
111 : o 
KVM start run
KVM_EXIT_MMIO
32 :   
KVM start run
KVM_EXIT_MMIO
119 : w 
KVM start run
KVM_EXIT_MMIO
104 : h 
KVM start run
KVM_EXIT_MMIO
97 : a 
KVM start run
KVM_EXIT_MMIO
116 : t 
KVM start run
KVM_EXIT_MMIO
39 : ' 
KVM start run
KVM_EXIT_MMIO
115 : s 
KVM start run
KVM_EXIT_MMIO
32 :   
KVM start run
KVM_EXIT_MMIO
117 : u 
KVM start run
KVM_EXIT_MMIO
112 : p 
KVM start run
KVM_EXIT_MMIO
33 : ! 
KVM start run
KVM_EXIT_MMIO
10 : 
 
KVM start run
Exit Reason: KVM_EXIT_SYSTEM_EVENT
Cause: Shutdown
```