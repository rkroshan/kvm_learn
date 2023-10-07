## Install KVM on Ubuntu on x86
`sudo apt install qemu-kvm`

### if installing inside virtualbox/vmware
- make sure to disable Hyper-V from "Turn Windows features on/off"
- disable Hyper-V from cmd termial via admin `bcdedit /set hypervisorlaunchtype off`
- reboot the system
- in the vms processor tab enable "VT-X/VT-d support"
- to check kvm works okay in vm , try cmd `kvm-ok` , if it says cpu support virtualization we are good to go
- ref video: https://www.youtube.com/watch?v=6f1Qckg2Zx0


## To run kvm_code_struct
### kvm_code_struct run a x86 vm executing opcode stored in struct code
- make
- ./kvm_code_struct
- sample output
```
9
KVM_EXIT_HLT
```
## To run kvm_code_bin
### kvm_code_bin run a x86 vm executing binary code from test.bin
- make
- ./kvm_code_bin
- sample output
```
KVM start run
KVM_EXIT_IO
out port: 16, data: 0
KVM start run
KVM_EXIT_IO
out port: 16, data: 1
```

## To run kvm_code_bin_multi
### kvm_code_bin_multi runs a x86 vm multithread with n vcpus executing binary code from test.bin
- make
- ./kvm_code_bin_multi
- can change num of vcpus by changing the macro `NUM_VPCUS` in kvm_code_bin_multi.c
- sample output
```
./kvm_code_bin_multi
read size: 7
KVM start run
KVM start run
KVM start run
KVM_EXIT_IO
out port: 16, data: 0 cpuid: 3
KVM_EXIT_IO
out port: 16, data: 0 cpuid: 0
KVM start run
KVM_EXIT_IO
out port: 16, data: 0 cpuid: 1
KVM_EXIT_IO
out port: 16, data: 0 cpuid: 2
KVM start run
KVM start run
```