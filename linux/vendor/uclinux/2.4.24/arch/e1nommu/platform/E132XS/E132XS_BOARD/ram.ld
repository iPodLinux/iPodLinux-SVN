OUTPUT_FORMAT("coff-e1-big")
MEMORY
	{
        romvec   : ORIGIN = 0x2000000,  LENGTH = 0x0000400
        flash    : ORIGIN = 0xE0000000, LENGTH = 0x20000 
        eflash   : ORIGIN = 0x2200000,  LENGTH = 0x20000
        ram      : ORIGIN = 0x00000000, LENGTH = 0x1000000
        eram16MB : ORIGIN = 0x01000000, LENGTH = 0
        sram     : ORIGIN = 0x40000000, LENGTH = 0x40000
		iram     : ORIGIN = 0xC0000000, LENGTH = 0x4000
	}
/* No Idea what are all these below !!! NEEDED */
ENTRY(Main)
MEM0start   = 0x00000000;
MEM0size    = 0x40000000;
MEM1start   = 0x40000000;
MEM1size    = 0x40000000;
MEM2start   = 0x80000000;
MEM2size    = 0x40000000;
IRAMstart   = 0xC0000000;
IRAMsize    = 0x20000000;
MEM3start   = 0xE0000000;
MEM3size    = 0x20000000;
Stack1Reserve = 560;
_Stack1Size = DEFINED(_Stack1Size)? _Stack1Size : 1*1024;
_Stack2Size = DEFINED(_Stack2Size)? _Stack2Size : 16*1024;
_Stack1Base = DEFINED(_Stack1Base)? _Stack1Base : __bss_end;
_Stack2Base = DEFINED(_Stack2Base)? _Stack2Base : __bss_end + _Stack1Size + Stack1Reserve;
_Mem0HeapBase = DEFINED(_Mem0HeapBase)? _Mem0HeapBase : _Stack2Base + _Stack2Size;
_Mem1HeapBase = DEFINED(_Mem1HeapBase)? _Mem1HeapBase : 0;
Priority    = DEFINED(Priority)   ? Priority    : 31;
TextBase    = DEFINED(TextBase)   ? TextBase    : 0x8000;

SECTIONS
{
/*
	{
	_flashstart = . ;
	_romvec = . ;
	__rom_start = . ;
	} > romvec
.rom :
        {
        _start = . ;   
        } > flash
.eflash :
	{
	_flashend = . ; 
	} > eflash
.realvec :     
	{
	__ramvec = . ;
	__ram_start = . ;
        } > ramvec
*/
  .G6 (DEFINED(G6Base) ? G6Base : 0x9000) : {
    *(.G6)
  }
  .G7 (DEFINED(G7Base) ? G7Base : 0x40001000) : {
    *(.G7)
  }
  .G8 (DEFINED(G8Base) ? G8Base : 0xC0000000) : {
    *(.G8)
  }
  .G9 (DEFINED(G9Base) ? G9Base : 0) : {
    *(.G9)
  }
  .G10 (DEFINED(G10Base) ? G10Base : 0) : {
    *(.G10)
  }
  .G11 (DEFINED(G11Base) ? G11Base : 0) : {
    *(.G11)
  }
  .G12 (DEFINED(G12Base) ? G12Base : 0) : {
    *(.G12)
  }
  .G13 (DEFINED(G13Base) ? G13Base : 0) : {
    *(.G13)
  }

.text TextBase :         /* Text section */
	{
    __stext = . ;
	text_start = . ;
       	*(.text)
		*(.text.*)
		*(.rodata)
	. = ALIGN(0x4) ;
		*(.kstrtab)
	. = ALIGN(16);          /* Exception table              */
	___start___ex_table = .;
		*(__ex_table)
	___stop___ex_table = .;

        ___start___ksymtab = .;  /* Kernel symbol table          */
		 *(__ksymtab)
	___stop___ksymtab = .;

	__etext = . ;
	___data_rom_start = ALIGN ( 4 ) ;
    G6Size = SIZEOF(.G6);
    G7Size = SIZEOF(.G7);
    G8Size = SIZEOF(.G8);
    G9Size = SIZEOF(.G9);
    G10Size = SIZEOF(.G10);
    G11Size = SIZEOF(.G11);
    G12Size = SIZEOF(.G12);
    G13Size = SIZEOF(.G13);

        } > ram
.data :
	{ 
	__sdata = . ;
	__data_start = . ;

        . = ALIGN(0x2000) ;
/*        *(.data.init_task) */
        *(.task_data)
        . = ALIGN(0x2000) ;

        . = ALIGN(0x2000) ;
/*        *(.data.task_init) */
        *(.tasks)
        . = ALIGN(0x2000) ;

        *(.data)
        *(.data.*)
        *(.setup.init)
/*        *(.exitcal.exit) */
        *(.exit.*) /* It should be .exitcall.exit */ 

        . = ALIGN(4096) ;
        ___init_begin = .;
        *(.text.init)
        *(.data.init)
        . = ALIGN(16);
        ___setup_start = .;
        *(.setup.init)
        *(.setup.*)
        ___setup_end = .;
        ___initcall_start = .;
        *(.init.*) /* It should be .initcall.init */ 
/*        *(.initcall.init) */
        . = ALIGN(4) ;
        ___initcall_end = .;
        ___init_end = .;
		*(.initrd);
    	. = ALIGN(4096);
	__edata = . ;
	edata = ALIGN( 0x10 ) ;
        } > ram
.bss :
        {
	__sbss = ALIGN( 0x10 ) ;
	__bss_start = ALIGN( 0x10 ) ;
	___data_end = ALIGN( 0x10 ) ;
	*(.bss)
	*(COMMON)
	__ebss = . ;
	__bss_end = . ;
	end = ALIGN( 0x10 ) ;
	__end = ALIGN( 0x10 ) ;
	} > ram

.eram16MB :    
	{
	___ramend = . - 0x7000;
	} > eram16MB

  .stab  0 (NOLOAD) :
  {
    [ .stab ]
  }
  .stabstr  0 (NOLOAD) :
  {
    [ .stabstr ]
  }

  _GOT_  0 (NOLOAD) :
  {
    [ _GOT_ ]
  }
}
