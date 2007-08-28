
INCLUDE arch/m68knommu/platform/5272/uC5272/romfs.ld

MEMORY {
	bvec   : ORIGIN = 0x00020000, LENGTH = 0x00000400
	ram    : ORIGIN = 0x00020400, LENGTH = 0x007e0000
	flash  : ORIGIN = 0x10c00000, LENGTH = 0x00400000
	eram   : ORIGIN = 0x00800000, LENGTH = 0     
}

SECTIONS {

        /* the bvec section used to trigger the bootloader */
	.romvec :
	{
		_romvec = . ;
	} > bvec

	.text : {
		_stext = . ;
        *(.text)
		*(.text.exit)
		*(.text.lock)
		*(.exitcall.exit)
		*(.rodata)
        *(.modinfo)
		. = ALIGN(0x4) ;
		*(.kstrtab)
		. = ALIGN(16);          /* Exception table              */
		__start___ex_table = .;
		*(__ex_table)
		__stop___ex_table = .;

		__start___ksymtab = .;  /* Kernel symbol table          */
		*(__ksymtab)
		__stop___ksymtab = .;
		. = ALIGN(4) ;
		_etext = . ;
	} > ram

	.data BLOCK(0x4) : {
		. = ALIGN(4);
		_sdata = . ;
		__data_start = . ;
        *(.data)
		*(.data.exit)
		. = ALIGN(8192) ;
        *(.data.init_task)
		. = ALIGN(8192) ;
		_edata = . ;
	} > ram

	.init BLOCK(4096) : {
		. = ALIGN(4096);
		__init_begin = .;
		*(.text.init)
		*(.data.init)
		. = ALIGN(16);
		__setup_start = .;
		*(.setup.init)
		__setup_end = .;
		__initcall_start = .;
		*(.initcall.init)
		. = ALIGN(4);
		__initcall_end = .;
		__init_end = .;
	} > ram

	.bss BLOCK(0x4) : {
		. = ALIGN(4);
		_sbss = . ;
		*(.bss)
		*(COMMON)
		. = ALIGN(4) ;
		_ebss = . ;
		_end = . ;
	} > ram

        .eram :
        {
        __ramend = . ;
        /*_ramend = . ; this will break the application */
        } > eram

        .fs :   {
                _romfs = ABSOLUTE (.);
                                     /* see the makefile on how this is used */
                . = . + romfs_size;  /* romfs_size must be calcuated */
                _end_romfs = ABSOLUTE (.); /* this seems to be important !! */
        } > ram

	.flash : {
		_flashstart = ABSOLUTE (.);
		. = . + 131072;
		_image_start = ABSOLUTE (.);
	
	} > flash
}
