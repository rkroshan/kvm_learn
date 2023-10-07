/* Sample code for /dev/kvm API
 *
 * Copyright (c) 2015 Intel Corporation
 * Author: Josh Triplett <josh@joshtriplett.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#include <err.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

/* https://lwn.net/Articles/658511/ https://kernel.org/doc/Documentation/virtual/kvm/api.txt
this file creates a x86 based VM whose job is to run below code mentioned in code struct (it is taken from object file)
to calculate sum of 2 digits and 
out it on a io port and 
hlt*/

int main(void)
{
    int kvm; /*file descriptors for kvm*/
    int vmfd; /*file descriptors for vm*/ 
    int vcpufd; /*file descriptors for vpcu*/ 
    int ret;

    const uint8_t code[] = {
        0xba, 0xf8, 0x03, /* mov $0x3f8, %dx */ /*dx = 0x3f8 , the port no */
        0x00, 0xd8,       /* add %bl, %al */    /*al = al+bl*/
        0x04, '0',        /* add $'0', %al */   /*al = al + 48 , to convert to ascii*/
        0xee,             /* out %al, (%dx) */  /*out it to port 0x3f8*/
        0xb0, '\n',       /* mov $'\n', %al */  /*al = '\n'*/
        0xee,             /* out %al, (%dx) */  /*out \n to 0x3f8*/
        0xf4,             /* hlt */             /*hlt instruction*/
    };
    uint8_t *mem;
    struct kvm_sregs sregs;
    size_t mmap_size;
    struct kvm_run *run;

    kvm = open("/dev/kvm", O_RDWR | O_CLOEXEC); /*We need read-write access to the device to set up a virtual machine and close the fd on exec* syscalls*/
    if (kvm == -1)
        err(1, "/dev/kvm"); /*unable to access kvm*/

    /* Make sure we have the stable version of the API == 12*/
    ret = ioctl(kvm, KVM_GET_API_VERSION, NULL);
    if (ret == -1)
        err(1, "KVM_GET_API_VERSION");
    if (ret != 12)
        errx(1, "KVM_GET_API_VERSION %d, expected 12", ret);

    vmfd = ioctl(kvm, KVM_CREATE_VM, (unsigned long)0); /*create a vm, 0 - Implies default size, 40bits physical address size*/
    if (vmfd == -1)
        err(1, "KVM_CREATE_VM");

    /* Allocate one aligned page of guest memory to hold the code. it is not backed by any file/fd stating from offset 0*/
    mem = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (!mem)
        err(1, "allocating guest memory");
    memcpy(mem, code, sizeof(code)); /*copy the code into this mem*/

    /* Map it to the second page frame (to avoid the real-mode IDT at 0). */
    struct kvm_userspace_memory_region region = {
        .slot = 0, /*provides an integer index identifying each region of memory we hand to KVM; calling KVM_SET_USER_MEMORY_REGION again with the same slot will replace this mapping*/
        .guest_phys_addr = 0x1000, /*specifies the base "physical" address as seen from the guest*/
        .memory_size = 0x1000,
        .userspace_addr = (uint64_t)mem, /*points to the backing memory in our process that we allocated with mmap()*/
    };
    ret = ioctl(vmfd, KVM_SET_USER_MEMORY_REGION, &region); /*set the region*/
    if (ret == -1)
        err(1, "KVM_SET_USER_MEMORY_REGION");

    vcpufd = ioctl(vmfd, KVM_CREATE_VCPU, (unsigned long)0); /*create a virtual CPU to run that code*/
    /*A KVM virtual CPU represents the state of one emulated CPU, including processor registers and other execution state.*/
    if (vcpufd == -1)
        err(1, "KVM_CREATE_VCPU");

    /*Each virtual CPU has an associated struct kvm_run data structure, 
    used to communicate information about the CPU between the kernel and user space. 
    In particular, whenever hardware virtualization stops (called a "vmexit"), 
    such as to emulate some virtual hardware, 
    the kvm_run structure will contain information about why it stopped. 
    We map this structure into user space using mmap(), but first, we need to know how much memory to map, 
    which KVM tells us with the KVM_GET_VCPU_MMAP_SIZE ioctl():*/
    /* Map the shared kvm_run structure and following data. */
    ret = ioctl(kvm, KVM_GET_VCPU_MMAP_SIZE, NULL);
    if (ret == -1)
        err(1, "KVM_GET_VCPU_MMAP_SIZE");
    mmap_size = ret;
    /*mmap size typically exceeds that of the kvm_run structure, 
    as the kernel will also use that space to store other transient structures 
    that kvm_run may point to. But it should not be small than run struct*/
    if (mmap_size < sizeof(*run))
        errx(1, "KVM_GET_VCPU_MMAP_SIZE unexpectedly small");
    run = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, vcpufd, 0); /*map the struct kvm_run into vcpufd starting from offset 0 upto mmap_size, so it is sahred between vcpu and we also see the same thing*/
    if (!run)
        err(1, "mmap vcpu");

    /* we zero the base and selector fields which together determine 
    what address in memory the segment points to. 
    To avoid changing any of the other initial "special" register states, 
    we read them out, change cs, and write them back:*/
    /* Initialize CS to point at 0, via a read-modify-write of sregs. */
    ret = ioctl(vcpufd, KVM_GET_SREGS, &sregs);
    if (ret == -1)
        err(1, "KVM_GET_SREGS");
    sregs.cs.base = 0;
    sregs.cs.selector = 0;
    ret = ioctl(vcpufd, KVM_SET_SREGS, &sregs);
    if (ret == -1)
        err(1, "KVM_SET_SREGS");

    /*For the standard registers, we set most of them to 0, 
    other than our initial instruction pointer (pointing to our code at 0x1000, relative to cs at 0), 
    our addends (2 and 7), and the initial state of the flags 
    (specified as 0x2 by the x86 architecture; starting the VM will fail with this not set):*/
    struct kvm_regs regs = {
        .rip = 0x1000,
        .rax = 2,
        .rbx = 7,
        .rflags = 0x2,
    };
    ret = ioctl(vcpufd, KVM_SET_REGS, &regs);
    if (ret == -1)
        err(1, "KVM_SET_REGS");

    /* Repeatedly run code and handle VM exits. */
    while (1) {
        ret = ioctl(vcpufd, KVM_RUN, NULL); /*start running instructions with the VCPU, using the KVM_RUN ioctl()*/
        if (ret == -1)
            err(1, "KVM_RUN");
        /*To handle the exit, we check run->exit_reason to see why we exited. 
        This can contain any of several dozen exit reasons, 
        which correspond to different branches of the union in kvm_run*/
        switch (run->exit_reason) {
        case KVM_EXIT_HLT: /*due to HLT instruction*/
            puts("KVM_EXIT_HLT");
            return 0;
        case KVM_EXIT_IO: /*due to io*/
            if (run->io.direction == KVM_EXIT_IO_OUT && run->io.size == 1 && run->io.port == 0x3f8 && run->io.count == 1)
                putchar(*(((char *)run) + run->io.data_offset)); /*what they want to right we will put on to stdout*/
            else
                errx(1, "unhandled KVM_EXIT_IO");
            break;
        case KVM_EXIT_FAIL_ENTRY:
        /* it indicates that the underlying hardware virtualization mechanism (VT in this case) 
        can't start the VM because the initial conditions don't match its requirements. 
        (Among other reasons, this error will occur if the flags register does not have bit 0x2 set, 
        or if the initial values of the segment or task-switching registers fail various setup criteria.) 
        The hardware_entry_failure_reason does not actually distinguish many of those cases, 
        so an error of this type typically requires a careful read through the hardware documentation.*/
            errx(1, "KVM_EXIT_FAIL_ENTRY: hardware_entry_failure_reason = 0x%llx",
                 (unsigned long long)run->fail_entry.hardware_entry_failure_reason);
        case KVM_EXIT_INTERNAL_ERROR:
        /*KVM_EXIT_INTERNAL_ERROR indicates an error from the Linux KVM subsystem rather than from the hardware.
        e.g. The run->internal.suberror value KVM_INTERNAL_ERROR_EMULATION indicates that the VM encountered an instruction 
        it doesn't know how to emulate, which most commonly indicates an invalid instruction
        */
            errx(1, "KVM_EXIT_INTERNAL_ERROR: suberror = 0x%x", run->internal.suberror);
        default:
            errx(1, "exit_reason = 0x%x", run->exit_reason);
        }
    }
}