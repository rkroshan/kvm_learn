CC=gcc
CPPFLAGS=-g -Wall -Wextra -Werror
LDFLAGS=

all: clean kvm_code_bin_multi test.bin run

kvm_code_bin_multi:
	$(CC) $(CPPFLAGS) kvm_code_bin_multi.c -o kvm_code_bin_multi -lpthread

test.bin: test.o
	ld -m elf_i386 --oformat binary -N -e _start -Ttext=0x10000 -o test.bin test.o

test.o: test.S
	as -32 test.S -o test.o

run:
	./kvm_code_bin_multi

clean:
	rm -rf kvm_code_bin_multi
	rm -rf test.bin test.o