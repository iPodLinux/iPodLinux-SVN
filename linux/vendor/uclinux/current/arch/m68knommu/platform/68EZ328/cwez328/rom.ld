MEMORY 
	{
        romvec : ORIGIN = 0x10000000, LENGTH = 0x00000400
        flash  : ORIGIN = 0x10000400, LENGTH = 0x400000 - 0x400
        eflash : ORIGIN = 0x10400000, LENGTH = 1
        ramvec : ORIGIN = 0x00000000, LENGTH = 0x400
        ram    : ORIGIN = 0x00000400, LENGTH = 0x00400000 - 0x00000400
        eram   : ORIGIN = 0x00400000, LENGTH = 1
	}

INCLUDE arch/m68knommu/platform/68EZ328/cwez328/fixed.ld
