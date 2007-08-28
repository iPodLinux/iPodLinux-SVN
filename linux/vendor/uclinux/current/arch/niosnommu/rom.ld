MEMORY 
	{
	flash  : ORIGIN = 0x00100000, LENGTH = 0x100000
	eflash : ORIGIN = 0x00200000, LENGTH = 1
	ramvec : ORIGIN = 0x00040000, LENGTH = 0x100
	ram    : ORIGIN = 0x00040100, LENGTH = 0x03FF00
	eram   : ORIGIN = 0x00080000, LENGTH = 1
	}

SECTIONS
{
        .text :
	{
	_romvec = . ;
	__rom_start = . ;
	*(.romvec)
	_stext = . ;
	text_start = . ;
        *(.text)
	*(.rodata)
	_etext = . ;
	__data_rom_start = ALIGN ( 4 ) ;
        } > flash
	.eflash :
	{
	_flashend = . ;
	} > eflash
	.ramvec :
	{
	__ram_start = . ;
	_ramvec = . ;
	} > ramvec
        .data :
	{
	_sdata = . ;
	__data_start = . ;
        *(.data)
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
	.eram :
	{
	_ramend = . ;
	} > eram
}
