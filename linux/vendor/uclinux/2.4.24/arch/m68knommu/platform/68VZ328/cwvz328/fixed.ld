SECTIONS
{
  .romvec :
	{
	_flashstart = . ;
	_romvec = . ;
	__rom_start = . ;
	} > romvec

  .text :
	{
	_stext = . ;
        	*(.text)
	. = ALIGN(0x4) ;
		*(.text.*)
	. = ALIGN(0x4) ;
		*(.exitcall.exit)
	. = ALIGN(0x4) ;
		*(.kstrtab)
	. = ALIGN(16);		/* Exception table */
	__start___ex_table = .;
		*(__ex_table)
	__stop___ex_table = .;

	__start___ksymtab = .; /* Kernel symbol table */
		*(__ksymtab)
	__stop___ksymtab = .;

	. = ALIGN(0x4);
	_etext = . ;
	__data_rom_start = . ;
        } > flash

  .eflash :
	{
	_flashend = . ;
	} > eflash

  .ramvec :
	{
	__ram_start = . ;
	__ramvec = . ;
	} > ramvec
  
  .data : 
	{
	. = ALIGN(0x4) ;
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

  .eram :
	{
        __ramend = . ;
	} > eram
}
