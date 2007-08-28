MEMORY 
	{
	romvec : ORIGIN = 0x10c00000, LENGTH = 0x10400
	flash  : ORIGIN = 0x10c10400, LENGTH = 0xfec00
	eflash : ORIGIN = 0x10d00000, LENGTH = 4
	ramvec : ORIGIN = 0x00000000, LENGTH = 0x400
	ram    : ORIGIN = 0x10000400, LENGTH = 0x100000-0x400
	eram   : ORIGIN = 0x10100000, LENGTH = 4
	}
INCLUDE arch/m68knommu/platform/68328/pilot/fixed.ld

