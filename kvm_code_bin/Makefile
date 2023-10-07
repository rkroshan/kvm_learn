CC=gcc
CPPFLAGS=-g -Wall -Wextra -Werror
LDFLAGS=

all: clean kvm_code_bin test.bin

kvm_code_bin:
	$(CC) $(CPPFLAGS) kvm_code_bin.c -o kvm_code_bin -lpthread

test.bin: test.o
	ld -m elf_i386 --oformat binary -N -e _start -Ttext=0x10000 -o test.bin test.o

test.o: test.S
	as -32 test.S -o test.o

clean:
	rm -rf kvm_code_bin
	rm -rf test.bin test.o