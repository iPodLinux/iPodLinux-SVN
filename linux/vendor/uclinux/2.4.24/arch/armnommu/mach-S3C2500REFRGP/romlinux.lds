/* ld script to make ARM Linux kernel
 * taken from the i386 version by Russell King
 * Written by Martin Mares <mj@atrey.karlin.mff.cuni.cz>
 *
 * Additional changes: 
 *  Copyright (C) 2001-2003 Arcturus Networks Inc.
 *                          by O.Zhadan <www.ArcturusNetworks.com>
 */

MEMORY
{
    ram : ORIGIN = 0x20000, LENGTH = 0x7debfc
    rom : ORIGIN = 0x1020000, LENGTH = 0x1dfffc
}

INCLUDE arch/armnommu/mach-S3C2500REFRGP/romfs.ld
OUTPUT_ARCH(arm)
ENTRY(stext)
SECTIONS
{
	.text 0x1020000 :
	{			/* Real text segment		*/
		_text = .;		/* Text and read-only data	*/
		_stext = .;
			*(.text.init)
			*(.text)
			*(.fixup)
			*(.gnu.warning)
			*(.text.lock)	/* out-of-line lock text */
			*(.rodata)
			*(.glue_7)
			*(.glue_7t)
			*(.kstrtab)
		. = ALIGN(16);
		__start___ex_table = .;	/* Exception table		*/
			*(__ex_table)
		__stop___ex_table = .;

		__start___ksymtab = .;	/* Kernel symbol table		*/
			*(__ksymtab)
		__stop___ksymtab = .;

		__start___kallsyms = .; /* All kernel symbols           */
			*(__kallsyms)
		__stop___kallsyms = .;


		*(.got)			/* Global offset table		*/

		_etext = .;		/* End of text section		*/
		
		. = ALIGN(32);
	} > rom

	.fs :	
	{
		_romfs = ABSOLUTE (.);
		. = . + romfs_size;
		_end_romfs = ABSOLUTE (.);
	} > rom

	
	.init 0x20000 :
	AT ( _end_romfs )
	{			/* Init code and data		*/
		
		__init_begin = ABSOLUTE(.);
		__proc_info_begin = ABSOLUTE(.);
			*(.proc.info)
		PROVIDE(__proc_info_end = .);
		__arch_info_begin = ABSOLUTE(.);
			*(.arch.info)
		PROVIDE(__arch_info_end = .);
			*(.data.init)
		. = ALIGN(16);
		__setup_start = ABSOLUTE(.);
			*(.setup.init)
		PROVIDE(__setup_end = .);
		__initcall_start = ABSOLUTE(.);
			*(.initcall.init)
		PROVIDE(__initcall_end = .);
		. = ALIGN(4096);
		PROVIDE(__init_end = .);
	} > ram

	/DISCARD/ : 
	{			/* Exit code and data		*/
		*(.text.exit)
		*(.data.exit)
		*(.exitcall.exit)
	} > ram

	
	.data : 
	AT ( _end_romfs + SIZEOF (.init))
	{
		. = ALIGN(8192);
		__data_start = ABSOLUTE(.);
		__data_rom_start = ABSOLUTE(.);
		/*
		 * first, the init task union, aligned
		 * to an 8192 byte boundary.
		 */
		*(.init.task)

		/*
		 * then the cacheline aligned data
		 */
		. = ALIGN(32);
		*(.data.cacheline_aligned)

		/*
		 * and the usual data section
		 */
		*(.data)
		CONSTRUCTORS

		PROVIDE(_edata = .);
		
	} > ram

	.bss : {
		__bss_start = ABSOLUTE(.);	/* BSS				*/
		PROVIDE(bss_start = .);
		*(.bss)
		*(COMMON)
		PROVIDE(_end = .);
		__bss_end = ABSOLUTE(.);
		. = ALIGN(8192);
		PROVIDE(_end_kernel = .);
	} > ram

					/* Stabs debugging sections.	*/
	.stab 0 : { *(.stab) }
	.stabstr 0 : { *(.stabstr) }
	.stab.excl 0 : { *(.stab.excl) }
	.stab.exclstr 0 : { *(.stab.exclstr) }
	.stab.index 0 : { *(.stab.index) }
	.stab.indexstr 0 : { *(.stab.indexstr) }
	.comment 0 : { *(.comment) }
}
