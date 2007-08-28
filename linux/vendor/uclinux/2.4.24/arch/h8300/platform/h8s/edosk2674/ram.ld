/* EDOSK-2674R RAM */

OUTPUT_ARCH(h8300s)
ENTRY("__start")

MEMORY 
	{
	ram    : ORIGIN = 0x400000, LENGTH = 0xc00000-0x400000
	eram   : ORIGIN = 0xc00000, LENGTH = 0
	}

SECTIONS
{
	.bootvec :
	{
	*(.bootvec)
	} > ram
        .text :
	{
	__stext = . ;
        	*(.text)
		*(.text.exit)
		*(.text.lock)
	. = ALIGN(0x4) ;
		*(.kstrtab)
	. = ALIGN(0x4) ;
		*(.rodata*)
	. = ALIGN(16);          /* Exception table              */
	___start___ex_table = .;
		*(__ex_table)
	___stop___ex_table = .;

        ___start___ksymtab = .;  /* Kernel symbol table          */
		 *(__ksymtab)
	___stop___ksymtab = .;

	. = ALIGN(0x4) ;
	__etext = . ;
        } > ram
	.data : 
	{
	. = ALIGN(0x2000) ;
		*(.data.init_task)
	__sdata = . ;
	___data_start = . ;
		*(.data)
		*(.data.exit)	
		*(.exitcall.exit)
        ___init_begin = .;
	        *(.text.init)
        	*(.data.init)
        . = ALIGN(16);
        ___setup_start = .;
	        *(.setup.init)
        ___setup_end = .;
        ___initcall_start = .;
        	*(.initcall.init)
        . = ALIGN(4) ;
        ___initcall_end = .;
        ___init_end = .;
	__edata = . ;
	} > ram
	.bss :
	{
	. = ALIGN(0x4) ;
	__sbss = . ;
		*(.bss)
	. = ALIGN(0x4) ;
		*(COMMON)
	. = ALIGN(0x4) ;
	__ebss = . ;
	__end = . ;
	__ramstart = .;
	} > ram
        .dummy2 :
        {
        COMMAND_START = . - 0x200 ;
	__ramend = . ;
	} > eram
}
