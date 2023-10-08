/*For Qemu Virt Aarch64 Arm SOC*/
/*https://github.com/qemu/qemu/blob/master/hw/arm/virt.c#L147C6-L147C15*/
volatile unsigned int * const UART0DR = (unsigned int *) 0x09000000;
 
void print_uart0(const char *s) {
    while(*s != '\0') { 		/* Loop until end of string */
         *UART0DR = (unsigned int)(*s); /* Transmit char */
          s++;			        /* Next char */
    }
}
 
void main() {
     print_uart0("Hello what's up!\n");
}