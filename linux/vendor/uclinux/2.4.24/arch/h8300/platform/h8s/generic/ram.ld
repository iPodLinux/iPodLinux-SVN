OUTPUT_ARCH(h8300s)
ENTRY("__start")

MEMORY 
	{
	ram    : ORIGIN = 0x400000, LENGTH = 0xA0000
/*	rdisk  : ORIGIN = 0x4A0000, LENGTH = 0x70000 */
/*	uram   : ORIGIN = 0x510000, LENGTH = 0xF0000 */
	uram   : ORIGIN = 0x4A0000, LENGTH = 0x160000
	eram   : ORIGIN = 0x600000, LENGTH = 0
	}

SECTIONS
{
	.bootvec :
	{
	*(.bootvec)
	} > ram
        .text :
	{
	__stext = . ;
        *(.text)
	__etext = . ;
        } > ram
	.rodata :
	{
	___data_rom_start = ALIGN ( 4 ) ;
	} > ram
	.erom :
	{
	__erom = . ;
	} > ram
	.data : 
	{
	__ramstart = . ;
	__sdata = . ;
	___data_start = . ;
        *(.data)
	__edata = . ;
	edata = ALIGN( 0x10 ) ;
	___data_end = ALIGN( 0x10 ) ;
	} > ram
        .bss :
        {
	__sbss = . ;
	___bss_start = . ;
	*(.bss)
	*(COMMON)
	__ebss = . ;
	___bss_end = . ;
	} > ram
/*
	.rootimg :
	{
	__rootimage = . ;
	} > rdisk
*/
	.dummy1 :
	{
	end = ALIGN( 0x10 ) ;
	__end = ALIGN( 0x10 ) ;
	} > uram
        .dummy2 :
        {
        _COMMAND_START = . - 0x200 ;
	__ramend = . ;
	} > eram
}
