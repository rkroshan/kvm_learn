CC=gcc
CPPFLAGS=-g -Wall -Wextra -Werror
LDFLAGS=

all: clean kvm_code_struct

kvm_code_struct:
	$(CC) $(CPPFLAGS) kvm_code_struct.c -o kvm_code_struct

clean:
	rm -rf kvm_code_struct