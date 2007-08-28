MEMORY
	{
        romvec : ORIGIN = 0x2000000, LENGTH = 0x0000400
        flash  : ORIGIN = 0x2000400, LENGTH = 0x0200000 - 0x0000400
        eflash : ORIGIN = 0x2200000, LENGTH = 0
        ramvec : ORIGIN = 0x0000000, LENGTH = 0x0000400
        ram    : ORIGIN = 0x0000400, LENGTH = 0x800000 - 0x0000400
        eram   : ORIGIN = 0x0800000, LENGTH = 0
	}
INCLUDE arch/m68knommu/platform/68VZ328/cwvz328/fixed.ld

