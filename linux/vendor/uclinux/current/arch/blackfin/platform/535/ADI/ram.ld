OUTPUT_FORMAT("elf32-little-nisa")
OUTPUT_ARCH(nisa)
ENTRY(start)
MEMORY 
	{
	ram		: ORIGIN = 0x1000,        LENGTH = 0x1000000
	l2_sram		: ORIGIN = 0xF0000000, LENGTH = 0x40000
	l1_data_a	: ORIGIN = 0xFF800000, LENGTH = 0x4000
	l1_data_b	: ORIGIN = 0xFF900000, LENGTH = 0x4000
	l1_code		: ORIGIN = 0xFFA00000, LENGTH = 0x4000
	l1_scratch	: ORIGIN = 0xFFB00000, LENGTH = 0x1000
	}

SECTIONS
{
  . = 0x1000 ;
  .text : {
	  _text = .;                    /* Text and read-only data */
	  text_start = . ;
	  __text_start = . ;
          *(.text)
          *(.text.*)
	  *($code)
          *(.fixup)		
          *(__ex_table)	
	  . = ALIGN(4) ;
	  _etext = . ;
	  etext = . ;
	} > ram
  
	
  .data :
	{
	  _sdata = . ;
	  __data_start = . ;

	  . = ALIGN(0x2000) ;
	  *(.data.init_task)
	  . = ALIGN(0x2000) ;

      *(.rodata)
      *(.data)
      *(.data.*)	
      . = ALIGN(4096) ;
      ramdisk_begin = .;
      *(.ramdisk)
      ramdisk_end = .;

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
	  edata = . ;
	} > ram

  .bss :
    {
	  . = ALIGN(16) ;
	  _sbss = . ;
	  __bss_start = . ;
	  __data_end = . ;
	  *(.bss)
	  *(COMMON)
	  . = ALIGN(16) ;
	  end = . ;
	  _ebss = . ;	  
	  _end = . ;
    } > ram

  . = 0x1000000;
  .eram :
    {
	__ramend = . ;
    } > ram






  . = 0xF0000000 ;
  .l2ram :
    {
	_l2_sram = . ;
    } > l2_sram


  . = 0xFF800000 ;
  .l1data1 :
    {
	_l1_data_a = . ;
    } > l1_data_a


  . = 0xFF900000 ;
  .l1data2 :
    {
	_l1_data_b = . ;
    } > l1_data_b


  . = 0xFFA00000 ;
  .l1code :
    {
	_l1_code = . ;
    } > l1_code

  . = 0xFFB00000 ;
  .l1scratch :
    {
	_l1_scratch = . ;
    } > l1_scratch

}
