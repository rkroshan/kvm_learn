/*
 * KVM x86 VM multicore.
 * author: rkroshan 
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <pthread.h>
#include <linux/kvm.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <err.h>

#define KVM_DEVICE "/dev/kvm"
#define RAM_SIZE 0x100000
#define CODE_START 0x1000
#define BINARY_FILE "test.bin"
#define NUM_VPCUS   4

struct kvm {
   int dev_fd;	/*device file descriptor*/
   int vm_fd;   /*vm file descriptor*/
   __u64 ram_size;  /*vm ram size*/
   __u64 ram_start; /*vm ram start*/
   int kvm_version; /*kvm version*/
   struct kvm_userspace_memory_region mem; /*user memory region*/
   struct vcpu *vcpus; /*vpcu struct pointer*/
   int vcpu_number; /*number of vpcus*/
};

struct vcpu {
    int vcpu_id; /*vpcu index*/
    int vcpu_fd; /*vpcu file descriptor*/
    pthread_t vcpu_thread; /*vpcu thread*/
    struct kvm_run *kvm_run; /*kvm run struct per vpcu*/
    int kvm_run_mmap_size; /*kvm run struct mmap size*/
    struct kvm_regs regs; /*kvm regs struct*/
    struct kvm_sregs sregs; /*kvm special regs struct*/
    void *(*vcpu_thread_func)(void *); /*vpcu thread function*/
};

/*function to setup reset values for vcpu regs and special regs*/
void kvm_reset_vcpu (struct vcpu *vcpu) {
	if (ioctl(vcpu->vcpu_fd, KVM_GET_SREGS, &(vcpu->sregs)) < 0) {
        /*get the kvm special regs for the vpcu*/
		perror("can not get sregs\n");
		exit(1);
	}
    /*setting up global descriptor table information for each segment*/
    /*since it is a simple program pointing every one to same offset*/
	vcpu->sregs.cs.selector = CODE_START;
	vcpu->sregs.cs.base = CODE_START * 16;
	vcpu->sregs.ss.selector = CODE_START;
	vcpu->sregs.ss.base = CODE_START * 16;
	vcpu->sregs.ds.selector = CODE_START;
	vcpu->sregs.ds.base = CODE_START *16;
	vcpu->sregs.es.selector = CODE_START;
	vcpu->sregs.es.base = CODE_START * 16;
	vcpu->sregs.fs.selector = CODE_START;
	vcpu->sregs.fs.base = CODE_START * 16;
	vcpu->sregs.gs.selector = CODE_START;

	if (ioctl(vcpu->vcpu_fd, KVM_SET_SREGS, &vcpu->sregs) < 0) { /*set*/
		perror("can not set sregs");
		exit(1);
	}

	vcpu->regs.rflags = 0x0000000000000002ULL; /*necessary to run the VM in x86*/
	vcpu->regs.rip = 0; /*instruction pointer starts from zero*/
	vcpu->regs.rsp = 0xffffffff; /*stack pointer at 4GB*/
	vcpu->regs.rbp= 0; /*base pointer at 0*/

	if (ioctl(vcpu->vcpu_fd, KVM_SET_REGS, &(vcpu->regs)) < 0) { /*set*/
		perror("KVM SET REGS\n");
		exit(1);
	}
}

void *kvm_cpu_thread(void *data) { /*per vpcu function*/
    struct vcpu *vcpu = (struct vcpu*)data;
	int ret = 0;
	kvm_reset_vcpu(vcpu); /*initialize vpcu regs*/

	while (1) { /*starts the VM and loop to catch vmexit reasons and then resume the vm*/
		printf("KVM start run\n");
		ret = ioctl(vcpu->vcpu_fd, KVM_RUN, 0); /*starts the vm*/
	
		if (ret < 0) {
			fprintf(stderr, "KVM_RUN failed\n");
			exit(1);
		}

		switch (vcpu->kvm_run->exit_reason) {
		case KVM_EXIT_UNKNOWN:
			printf("KVM_EXIT_UNKNOWN\n");
			break;
		case KVM_EXIT_DEBUG:
			printf("KVM_EXIT_DEBUG\n");
			break;
		case KVM_EXIT_IO: /*reason to exit when write to IO, we print in on stdout*/
            printf("KVM_EXIT_IO\n");
            printf("out port: %d, data: %d cpuid: %d\n", 
				vcpu->kvm_run->io.port,  
				*(int *)((char *)(vcpu->kvm_run) + vcpu->kvm_run->io.data_offset),
				vcpu->vcpu_id);
			sleep(1); /*sleep for a sec, who cares we are in vmexit :))*/
			break;
		case KVM_EXIT_MMIO:
			printf("KVM_EXIT_MMIO\n");
			break;
		case KVM_EXIT_INTR:
			printf("KVM_EXIT_INTR\n");
			break;
		case KVM_EXIT_SHUTDOWN:
			printf("KVM_EXIT_SHUTDOWN\n");
			break;
		case KVM_EXIT_FAIL_ENTRY:
        /* it indicates that the underlying hardware virtualization mechanism (VT in this case) 
        can't start the VM because the initial conditions don't match its requirements. 
        (Among other reasons, this error will occur if the flags register does not have bit 0x2 set, 
        or if the initial values of the segment or task-switching registers fail various setup criteria.) 
        The hardware_entry_failure_reason does not actually distinguish many of those cases, 
        so an error of this type typically requires a careful read through the hardware documentation.*/
            errx(1, "KVM_EXIT_FAIL_ENTRY: hardware_entry_failure_reason = 0x%llx",
                 (unsigned long long)vcpu->kvm_run->fail_entry.hardware_entry_failure_reason);
        case KVM_EXIT_INTERNAL_ERROR:
        /*KVM_EXIT_INTERNAL_ERROR indicates an error from the Linux KVM subsystem rather than from the hardware.
        e.g. The run->internal.suberror value KVM_INTERNAL_ERROR_EMULATION indicates that the VM encountered an instruction 
        it doesn't know how to emulate, which most commonly indicates an invalid instruction
        */
            errx(1, "KVM_EXIT_INTERNAL_ERROR: suberror = 0x%x", vcpu->kvm_run->internal.suberror);
        default:
            errx(1, "exit_reason = 0x%x", vcpu->kvm_run->exit_reason);
        }
	}
	return 0;
}


/*to load data from binary to a buffer*/
void load_binary(struct kvm *kvm) {
    int fd = open(BINARY_FILE, O_RDONLY); /*open the bin file*/

    if (fd < 0) {
        fprintf(stderr, "can not open binary file\n");
        exit(1);
    }

    int ret = 0;
    char *p = (char *)kvm->ram_start; /*addr from where bin will be kept*/

    while(1) {
        ret = read(fd, p, 4096); /*read upto 4096 bytes*/
        if (ret <= 0) {
            break;
        }
        printf("read size: %d\n", ret);
        p += ret; /*move pointer ahead of last offset upto which data is written*/
    }
}

/*utility function to initialize and open kvm device*/
struct kvm *kvm_init(void) {
    struct kvm *kvm = malloc(sizeof(struct kvm)); /*allocate mem for kvm struct*/
    kvm->dev_fd = open(KVM_DEVICE, O_RDWR); /*open kvm device and store the file descriptor*/

    if (kvm->dev_fd < 0) {
        perror("open kvm device fault: ");
        return NULL;
    }

    kvm->kvm_version = ioctl(kvm->dev_fd, KVM_GET_API_VERSION, 0); /*get the KVM API version should be 12*/

    return kvm;
}

/*utility function to clean up the allocated mem for kvm struct and close the fd*/
void kvm_clean(struct kvm *kvm) {
    assert (kvm != NULL);
    close(kvm->dev_fd);
    free(kvm);
}

/*function to create vm*/
int kvm_create_vm(struct kvm *kvm, int ram_size) {
    int ret = 0;
    kvm->vm_fd = ioctl(kvm->dev_fd, KVM_CREATE_VM, 0); /*create VM*/

    if (kvm->vm_fd < 0) {
        perror("can not create vm");
        return -1;
    }

    kvm->ram_size = ram_size;
    /* Allocate mem (aligned to page) of guest memory to hold the code. it is not backed by any file/fd stating from offset 0*/
    kvm->ram_start =  (__u64)mmap(NULL, kvm->ram_size, 
                PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, 
                -1, 0);

    if ((void *)kvm->ram_start == MAP_FAILED) {
        perror("can not mmap ram");
        return -1;
    }
    
    kvm->mem.slot = 0; /*provides an integer index identifying each region of memory we hand to KVM; calling KVM_SET_USER_MEMORY_REGION again with the same slot will replace this mapping*/
    kvm->mem.guest_phys_addr = 0; /*specifies the base "physical" address as seen from the guest*/
    kvm->mem.memory_size = kvm->ram_size; 
    kvm->mem.userspace_addr = kvm->ram_start; /*points to the backing memory in our process that we allocated with mmap()*/

    ret = ioctl(kvm->vm_fd, KVM_SET_USER_MEMORY_REGION, &(kvm->mem)); /*set the region*/

    if (ret < 0) {
        perror("can not set user memory region");
        return ret;
    }

    return ret;
}

/*function to close vm fd and unmap ram data*/
void kvm_clean_vm(struct kvm *kvm) {
    close(kvm->vm_fd);
    munmap((void *)kvm->ram_start, kvm->ram_size);
}

/*function to create vpcu*/
int kvm_init_vcpu(struct kvm *kvm, struct vcpu* vcpu, int vcpu_id, void *(*fn)(void *)) {
    vcpu->vcpu_id = vcpu_id;
    vcpu->vcpu_fd = ioctl(kvm->vm_fd, KVM_CREATE_VCPU, vcpu->vcpu_id); /*create vpcu*/

    if (vcpu->vcpu_fd < 0) {
        perror("can not create vcpu");
        return -1;
    }

    vcpu->kvm_run_mmap_size = ioctl(kvm->dev_fd, KVM_GET_VCPU_MMAP_SIZE, 0); /*get the mem size for vpcu*/

    if (vcpu->kvm_run_mmap_size < 0) {
        perror("can not get vcpu mmsize");
        return -1;
    }

    // printf("%d\n", vcpu->kvm_run_mmap_size);
    /*map the struct kvm_run into vcpufd starting from offset 0 upto mmap_size, so it is sahred between vcpu and we also see the same thing*/
    vcpu->kvm_run = mmap(NULL, vcpu->kvm_run_mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, vcpu->vcpu_fd, 0);

    if (vcpu->kvm_run == MAP_FAILED) {
        perror("can not mmap kvm_run");
        return -1;
    }

    vcpu->vcpu_thread_func = fn;
    return 0;
}

/*function to create vpcus*/
struct vcpu* kvm_create_vpcus(struct kvm* kvm, int num_vcpus, void *(*fn)(void *))
{
    struct vcpu *vcpus = malloc(num_vcpus * sizeof(struct vcpu));
    if(vcpus == NULL){
        printf("failed to allocate mem for vpcus\n");
        return NULL;
    }
    for(int vcpu_id = 0; vcpu_id < num_vcpus; vcpu_id++)
    {
        if(kvm_init_vcpu(kvm, &vcpus[vcpu_id], vcpu_id, fn) < 0){
            printf("failed to init vpcu %d\n", vcpu_id);
            return NULL;
        }
    }
    return vcpus;
}

/*function to unmap kvm_run to vcpu mem and close vcpu fd*/
void kvm_clean_vcpus(struct vcpu *vcpu, int num_vcpus) {
    for(int id=0;id<num_vcpus;id++)
    {
        munmap(vcpu[id].kvm_run, vcpu[id].kvm_run_mmap_size);
        close(vcpu[id].vcpu_fd);
    }
    free(vcpu);
}

/*function to run each vcpu of the vm per thread*/
void kvm_run_vm(struct kvm *kvm) {
    int i = 0;

    for (i = 0; i < kvm->vcpu_number; i++) { /*pthread_create create thread within process and calls kvm_cpu_thread function*/
        if (pthread_create(&(kvm->vcpus[i].vcpu_thread), (const pthread_attr_t *)NULL, kvm->vcpus[i].vcpu_thread_func, (void*)&kvm->vcpus[i]) != 0) {
            perror("can not create kvm thread");
            exit(1);
        }
    }

    for (i = 0; i < kvm->vcpu_number; i++) 
    {
        pthread_join(kvm->vcpus[i].vcpu_thread, NULL);
    }
}

int main(void) {
    int ret = 0;
    struct kvm *kvm = kvm_init();

    if (kvm == NULL) {
        fprintf(stderr, "kvm init fauilt\n");
        return -1;
    }

    if (kvm_create_vm(kvm, RAM_SIZE) < 0) {
        fprintf(stderr, "create vm fault\n");
        return -1;
    }

    load_binary(kvm);

    // only support one vcpu now
    kvm->vcpu_number = NUM_VPCUS;
    kvm->vcpus = kvm_create_vpcus(kvm, kvm->vcpu_number, kvm_cpu_thread);

    kvm_run_vm(kvm);

    kvm_clean_vcpus(kvm->vcpus, kvm->vcpu_number);
    kvm_clean_vm(kvm);
    kvm_clean(kvm);
    return ret;
}