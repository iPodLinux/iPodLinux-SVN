OUTPUT_FORMAT("elf32-nios", "elf32-nios", "elf32-nios")
OUTPUT_ARCH(nios)
ENTRY(_start)	/* Defined in head.S */

SECTIONS
{
        .text :
	{
	__ram_start = . ;
	_stext = . ;
	text_start = . ;
        	*(.text)
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
        __initcall_end = .;
        __init_end = .;
        . = ALIGN(4096) ;

	_edata = . ;
	_kernel_footprint = . -__ram_start ;
	edata = ALIGN( 0x10 ) ;
/*	. = ALIGN( 0x10 );		*/
/*	*(.romf)			*/
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
/*	end = ALIGN( 0x10 ) ;		*/
/*	_end = ALIGN( 0x10 ) ;		*/
	end = ALIGN( 0x1000 ) ;
	_end = ALIGN( 0x1000 ) ;
	} > ram

	.eram :
	{
	__ramend = . ;
	_ramend = . ;
	} > eram

 /* Sections to be discarded */
  /DISCARD/ : {
	*(.text.exit)
	*(.data.exit)
	*(.exitcall.exit)
	}

  /*
   * ------------------------------------------------------------
   * dvb say: "I'll leave all this stuff down here exactly
   * as I found it, for debugging info, without
   * understanding it."
   */

  /* Stabs debugging sections.  */
  .stab 0 : { *(.stab) }
  .stabstr 0 : { *(.stabstr) }
  .stab.excl 0 : { *(.stab.excl) }
  .stab.exclstr 0 : { *(.stab.exclstr) }
  .stab.index 0 : { *(.stab.index) }
  .stab.indexstr 0 : { *(.stab.indexstr) }
  .comment 0 : { *(.comment) }
  /* DWARF debug sections.
     Symbols in the DWARF debugging sections are relative to the beginning
     of the section so we begin them at 0.  */
  /* DWARF 1 */
  .debug          0 : { *(.debug) }
  .line           0 : { *(.line) }
  /* GNU DWARF 1 extensions */
  .debug_srcinfo  0 : { *(.debug_srcinfo) }
  .debug_sfnames  0 : { *(.debug_sfnames) }
  /* DWARF 1.1 and DWARF 2 */
  .debug_aranges  0 : { *(.debug_aranges) }
  .debug_pubnames 0 : { *(.debug_pubnames) }
  /* DWARF 2 */
  .debug_info     0 : { *(.debug_info) }
  .debug_abbrev   0 : { *(.debug_abbrev) }
  .debug_line     0 : { *(.debug_line) }
  .debug_frame    0 : { *(.debug_frame) }
  .debug_str      0 : { *(.debug_str) }
  .debug_loc      0 : { *(.debug_loc) }
  .debug_macinfo  0 : { *(.debug_macinfo) }
  /* SGI/MIPS DWARF 2 extensions */
  .debug_weaknames 0 : { *(.debug_weaknames) }
  .debug_funcnames 0 : { *(.debug_funcnames) }
  .debug_typenames 0 : { *(.debug_typenames) }
  .debug_varnames  0 : { *(.debug_varnames) }
  /* These must appear regardless of  .  */
}
