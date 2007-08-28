MEMORY
	{
        romvec : ORIGIN = 0x2000000, LENGTH = 0x0000400
        flash  : ORIGIN = 0x2000400, LENGTH = 0x0200000 - 0x0000400
        eflash : ORIGIN = 0x2200000, LENGTH = 0
        ramvec : ORIGIN = 0x0000000, LENGTH = 0x0000400
        framebuf : ORIGIN = 0x0000400, LENGTH = 0x002fc00
        eframebuf : ORIGIN = 0x0030000, LENGTH = 0
        ram    : ORIGIN = 0x0030000, LENGTH = 0x2000000 - 0x0030000
        eram8MB : ORIGIN = 0x0800000, LENGTH = 0
        eram32MB : ORIGIN = 0x2000000, LENGTH = 0
	}


SECTIONS
{
        .romvec :
	{
	_flashstart = . ;
	_romvec = . ;
	__rom_start = . ;
	} > romvec

	.eflash :
	{
	_flashend = . ;
	} > eflash
	.realvec :
	{
	__ramvec = . ;
	} > ramvec

	.framebuf :
	{
	__vram_start = . ;
	} > framebuf
	.eframebuf :
	{
	__vram_end = . ;
	} > eframebuf

	.romvec :
	{
	_romvec = . ;
	} > bvec
        .text :
	{
	__ram_start = . ;
	text_start = . ;
        	*(.text)
		*(.text.*)
		*(.rodata)
	. = ALIGN(0x4) ;
		*(.kstrtab)
	. = ALIGN(16);          /* Exception table              */
	__start___ex_table = .;
		*(__ex_table)
	__stop___ex_table = .;

        __start___ksymtab = .;  /* Kernel symbol table          */
		 *(__ksymtab)
	__stop___ksymtab = .;

	_etext = . ;
	__data_rom_start = ALIGN ( 4 ) ;
        } > ram
        .data :
	{
	_sdata = . ;
	__data_start = . ;

        . = ALIGN(0x2000) ;
        *(.data.init_task)
        . = ALIGN(0x2000) ;

        *(.data)
        *(.data.*)
        *(.setup.init)
        *(.exitcall.exit)

        . = ALIGN(4096) ;
        __init_begin = .;
        *(.text.init)
        *(.data.init)
        . = ALIGN(16);
        __setup_start = .;
        *(.setup.init)
        __setup_end = .;
        __initcall_start = .;
        *(.initcall.init)
        . = ALIGN(4) ;
        __initcall_end = .;
        __init_end = .;

	_edata = . ;
	edata = ALIGN( 0x10 ) ;
        } > ram
        .bss :
        {
	_sbss = ALIGN( 0x10 ) ;
	__bss_start = ALIGN( 0x10 ) ;
	__data_end = ALIGN( 0x10 ) ;
	*(.bss)
	*(COMMON)
	_ebss = . ;
	__bss_end = . ;
	end = ALIGN( 0x10 ) ;
	_end = ALIGN( 0x10 ) ;
	} > ram

  .eram8MB :
	{
	__ramend8MB = . ;
        __ramend = . ;
	} > eram8MB
  .eram32MB :
	{
	__ramend32MB = . ;
	} > eram32MB

}