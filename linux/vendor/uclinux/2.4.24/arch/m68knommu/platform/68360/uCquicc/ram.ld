MEMORY
{
    romvec   : ORIGIN = 0x00000000,                 LENGTH = 1028
    flash    : ORIGIN = 1028,                       LENGTH = 0x00080000 - 1028
    eflash   : ORIGIN = 0x00000000 + 0x00100000,    LENGTH = 1
    ramvec   : ORIGIN = 0x00200000,                 LENGTH = 1028
    ram      : ORIGIN = 0x00200000 + 1028,          LENGTH = 0x00080000 - 1028
    eram     : ORIGIN = 0x00200000 + 0x00080000,    LENGTH = 1
    dpram    : ORIGIN = 0xffffe000,                 LENGTH = 0x00002000
}

SECTIONS
{
    .dpram :
    {
        _dprbase = . ;
    } > dpram

    .romvec :
    {
        _romvec = . ;
        __flashstart = . ;
        *(.data.initvect)
        . = ALIGN(4);
    } > romvec

    .text :
    {
        text_start = . ;
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.fixup)
	*(.kstrtab)
	__start___ksymtab = . ;
	*(__ksymtab)
        __stop___ksymtab = . ;
	__start___ex_table = . ;
	*(___ex_table)
	__stop___ex_table = . ;
        . = ALIGN(4) ;_etext = . ;
        __data_rom_start = ALIGN ( 4 ) ;
    } > flash
   
    .eflash :
    {
        __flashend = . ;
    } > eflash


    .ramvec :
    {
        __ramstart = . ;
        _ramvec = . ;
    } > ramvec
   
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
        edata = .;
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

    .eram :
    {
        _boot_stack = . - 4;
        __ramend = . ;

    } > eram
}
