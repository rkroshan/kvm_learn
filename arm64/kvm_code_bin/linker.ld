ENTRY(_start)

SECTIONS
{
	.boot . : { boot.o(.text) }
	.text : { *(.text) }
	.data : { *(.data) }
	.bss : { *(.bss COMMON) }
	. = ALIGN(16);
	. = . + 0x1000; /* 4kB of stack memory */
	stack_top = .;
}
