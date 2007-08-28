MEMORY
	{
        romvec : ORIGIN = 0x2000000, LENGTH = 0x0000400
        flash  : ORIGIN = 0x2000400, LENGTH = 0x0200000 - 0x0000400
        eflash : ORIGIN = 0x2200000, LENGTH = 0
        ramvec : ORIGIN = 0x0000000, LENGTH = 0x0000400
        framebuf : ORIGIN = 0x0000400, LENGTH = 0x002fc00
        eframebuf : ORIGIN = 0x0030000, LENGTH = 0
        ram    : ORIGIN = 0x0030000, LENGTH = 0x2000000 - 0x0030000
        eram8MB : ORIGIN = 0x0600000, LENGTH = 0
        eram32MB : ORIGIN = 0x1e00000, LENGTH = 0
	}
MEMORY
	{
        romvec : ORIGIN = 0x01e00000, LENGTH = 1k
        flash  : ORIGIN = 0x01e00400, LENGTH = 2M - 1k
        eflash : ORIGIN = 0x02000000, LENGTH = 0
        ramvec : ORIGIN = 0x00000000, LENGTH = 1k
        ram    : ORIGIN = 0x00010000, LENGTH = 30M - 64k
        eram   : ORIGIN = 0x01e00000, LENGTH = 0
	}

INCLUDE arch/m68knommu/platform/68VZ328/dragonixvz/fixed.ld
