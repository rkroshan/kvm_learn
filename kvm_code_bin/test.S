# A test code for kvmsample

.globl _start
# cpu in 16 bit mode
    .code16 
_start:
# clear ax reg
    xorw %ax, %ax
    
loop1:
# write to port 0x10 ax value and increment it by one and repeat
    out %ax, $0x10
    inc %ax
    jmp loop1
