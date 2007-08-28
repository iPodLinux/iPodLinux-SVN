MEMORY
	{
        romvec : ORIGIN = 0x2000000, LENGTH = 0x0000400
        flash  : ORIGIN = 0x2000400, LENGTH = 0x0200000 - 0x0000400
        eflash : ORIGIN = 0x2200000, LENGTH = 0
        ramvec : ORIGIN = 0x0000000, LENGTH = 0x0000400
        framebuf : ORIGIN = 0x0000400, LENGTH = 0x002fc00
        eframebuf : ORIGIN = 0x0030000, LENGTH = 0
        ram    : ORIGIN = 0x0030000, LENGTH = 0x2000000 - 0x0030000
        eram8MB : ORIGIN = 0x0800000, LENGTH = 0
        eram32MB : ORIGIN = 0x2000000, LENGTH = 0
	}

INCLUDE arch/m68knommu/platform/68VZ328/dragonixvz/fixed.ld
