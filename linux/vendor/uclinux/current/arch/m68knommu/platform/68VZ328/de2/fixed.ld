SECTIONS {

	.text : {
		. = ALIGN(4) ;
		_stext = . ;
        	*(.text)
			*(.text.exit)
			*(.text.lock)
			*(.exitcall.exit)
			*(.rodata)
		. = ALIGN(4) ;
			*(.kstrtab)
		. = ALIGN(16);          /* Exception table              */
		__start___ex_table = .;
			*(__ex_table)
		__stop___ex_table = .;

		__start___ksymtab = .;  /* Kernel symbol table          */
			*(__ksymtab)
		__stop___ksymtab = .;
		. = ALIGN(4) ;
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
		. = ALIGN(4) ;
		_etext = . ;
		__data_rom_start = . ; /* only needed for CONFIG_BLK_DEV_BLKMEM */
	} > rom

	.data : AT(_etext) {
		. = ALIGN(4) ;
		_sdata = . ;
		__data_start = . ;
        	*(.data)
			*(.data.exit)
		. = ALIGN(0x2000) ;
        	*(.data.init_task)
		. = ALIGN(0x2000) ;
		_edata = . ;
	} > ram

	.bss : {
		. = ALIGN(4) ;
		_sbss = . ;
			*(.bss)
			*(COMMON)
		. = ALIGN(4) ;
		_ebss = . ;
	} > ram
}
