/* Simple development memory map for Xilinx Microblaze target.
   Assume that only the interrupt vectors and the small local data
   area go in LMB RAM, and that some mythical bootloader will 
   put the kernel into OPB RAM starting from 0xffe00000
*/

/* Plenty of room to do tricky stuff here.  e.g. for speed, we could
   put IRQ handlers and the OS scheduling code into LMB.
   That's just one idea, will look into it more later.
*/
  
MEMORY {
	/* 8K of LMB RAM */
	LMB  : ORIGIN = 0x00000000, LENGTH = 0x00002000
	/* 1MB of OPB SRAM */
	/* OPB  : ORIGIN = 0xFFE00000, LENGTH = 0x00100000 */

	/* 16 MB of OBP SDRAM */
	OPB  : ORIGIN = 0x80000000, LENGTH = 0x01000000 
}

SECTIONS {
	.text : {
		/* "untouchable" memory space starts here */
	        _kram_start = . ;
	
		_stext = . ;
        	*(.text)
			*(.text.exit)
			*(.text.lock)
			*(.exitcall.exit)
			*(.rodata)
		*(.modinfo)
		. = ALIGN (0x4) ;
			*(.kstrtab)
		. = ALIGN (16) ;	/* Exception table.  */
		__start___ex_table = . ;
			*(_ex_table)
		__stop___ex_table = . ;

		__start___ksymtab = . ; /* All kernel symbols */
		*(__ksymtab)
		__stop___ksymtab = . ;

                __start___kallsyms = .; /* All kernel symbols           */
                        *(__kallsyms)
                __stop___kallsyms = .;


		. = ALIGN (4) ;
		_etext = . ;
	} > OPB

	/* Copies of interrupt vector space. They get copied from here into the 
  	   proper location by mach_early_setup() */ 
	.intv : {
		_intv_load_start = . ;
	        	*(.intv.reset)	/* Reset vector */
			*(.intv.common) /* Vectors common to all microblaze proc. */
			*(.intv.mach)   /* Machine-specific int. vectors.  */
		_intv_load_end = . ;
	} > OPB


	/* sdata2 section can go anywhere, but must be word aligned
	   and SDA2_BASE must point to the middle of it */
	.sdata2 : {
		_ssrw = .;
		. = ALIGN(0x8);
		*(.sdata2)
	. = ALIGN(8);
	_essrw = .;
	_ssrw_size = _essrw - _ssrw;
	_KERNEL_SDA2_BASE_ = _ssrw + (_ssrw_size / 2);
	} > OPB


	.data ALIGN (0x4) : {

		_sdata = . ;
		__data_start = . ;
        	*(.data)
			*(.data.exit)
		. = ALIGN (16) ;
		*(.data.cacheline_aligned)
		. = ALIGN (0x2000) ;
        	*(.data.init_task)
		. = ALIGN (0x2000) ;
		_edata = . ;
	
		/* Reserve some low RAM for r0 based memory references */
		. = ALIGN(4) ;
		C_SYMBOL_NAME(r0_ram) = . ;
		. = . + 32 ;	/* 32 bytes should be enough */
	} > OPB

	/* Under the microblaze ABI, .sdata and .sbss must be contiguous */
	. = ALIGN(8);
	.sdata : {
		_ssro = .;
		*(.sdata)
	} > OPB

	. = ALIGN(4);
	.sbss : {
		_ssbss = . ;
		_sbss_start = .;
		*(.sbss)
		_ssbss_end = .;
		_esbss = . ;

	. = ALIGN(8);
	_essro = .;
	_ssro_size = _essro - _ssro;
	_KERNEL_SDA_BASE_ = _ssro + (_ssro_size / 2);
	
 	} > OPB
 
	/* Place init and bootmap sections last to reduce fragmentation 
	   when they are freed */
 	.init ALIGN (4096) : {
 		_init_start = . ;
 			*(.text.init)
 			*(.data.init)
 		. = ALIGN (16) ;
 		__setup_start = . ;
 			*(.setup.init)
 		__setup_end = . ;
 		__initcall_start = . ;
 			*(.initcall.init)
 		. = ALIGN (4) ;
 		__initcall_end = . ;
 		_init_end = . ;
 		/* Untouchable kernel ram ends here */
 		_kram_end = . ;
 	} > OPB
 
	/* Static room for the bootmap.  
	   FIXME migrate this to be dynamic, a la coldfire */
 	.bootmap ALIGN (4096) : {
 		_bootmap = . ;
		. = . + 4096 ;		/* enough for 128MB.   */
		_bootmap_end = . ;
	} > OPB

	/* .bss must go last, to be compatible with the way the romfs is handled */
	.bss ALIGN (0x4) : {
		_sbss = . ;
			*(.bss*)
			*(COMMON)
		. = ALIGN (4) ;
		_ebss = . ;
		_end = . ;
 	} > OPB
 
}

