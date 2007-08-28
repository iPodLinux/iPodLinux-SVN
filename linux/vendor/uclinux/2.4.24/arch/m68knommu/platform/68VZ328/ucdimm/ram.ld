MEMORY 
	{
	romvec : ORIGIN = 0x10c10000, LENGTH = 0x00000400
	flash  : ORIGIN = 0x10c10400, LENGTH = 0x00200000 - 0x00010400
	eflash : ORIGIN = 0x10e00000, LENGTH = 0
	ramvec : ORIGIN = 0x00000000, LENGTH = 0x00000400
	bvec   : ORIGIN = 0x00020000, LENGTH = 0x00000400
	ram    : ORIGIN = 0x00020400, LENGTH = 0x00800000 - 0x00020400
	eram   : ORIGIN = 0x00800000, LENGTH = 0
	}


SECTIONS
{
  .flashvec :
	{
	_flashstart = . ;
	__rom_start = . ;
	} > romvec


	.eflash :
	{
	_flashend = . ;
	} > eflash

  .ramvec :
	{
	__ramvec = . ;
	} > ramvec

	.romvec :
	{
	_romvec = . ;
	} > bvec

        .text :
	{
	__ram_start = . ;
	_stext = . ;
		*(.text)
	. = ALIGN(0x4) ;
		*(.text.*)
	. = ALIGN(0x4) ;
		*(.exitcall.exit)
	. = ALIGN(0x4) ;
		*(.kstrtab)
	. = ALIGN(16);          /* Exception table              */
	__start___ex_table = .;
		*(__ex_table)
	__stop___ex_table = .;

        __start___ksymtab = .;  /* Kernel symbol table          */
		 *(__ksymtab)
	__stop___ksymtab = .;

	. = ALIGN(0x4) ;
	_etext = . ;
        } > ram

        .data :
	{
	. = ALIGN(0x4) ;
	__data_rom_start = . ;
	_sdata = . ;
	__data_start = . ;

	. = ALIGN(0x4) ;
		*(.rodata)
	. = ALIGN(0x4) ;
        *(.data)
	. = ALIGN(0x4) ;
        *(.data.*)      

	. = ALIGN(0x4) ;
        __setup_start = .;
        *(.setup.init)
	. = ALIGN(0x4) ;
        __setup_end = .;

	. = ALIGN(0x4) ;
        __initcall_start = .;
        *(.initcall.init)
	. = ALIGN(0x4) ;
        __initcall_end = .;

	. = ALIGN(0x2000) ;
		*(.data.init_task)
	. = ALIGN(0x2000) ;

	_edata = . ;
        } > ram

        .bss :
        {
	. = ALIGN(0x4) ;
	_sbss = . ;
	*(.bss)
	. = ALIGN(0x4) ;
	*(COMMON)
	. = ALIGN(0x4) ;
	_ebss = . ;
	_end = . ;
	} > ram

  .fs :
	{
		__romfs_start = . ;
	} > ram

	.eram :
	{
	__ramend = . ;
	} > eram
}
