/*
  for a 32MB SDRAM:

	rom : ORIGIN = 0x01e00000, LENGTH = 2M

  for a 16MB SDRAM:

	rom : ORIGIN = 0x00e00000, LENGTH = 2M

  for a 8MB SDRAM:

	rom : ORIGIN = 0x00600000, LENGTH = 2M

  RAM size should now be set using the config options
  the size here should stay to the maximum possible for
  any DragonEngine board minus the HIMEM reserved space
*/

MEMORY {
	rom : ORIGIN = 0x00600000, LENGTH = 2M
	ram : ORIGIN = 0x00010000, LENGTH = 6M - 64k
}

INCLUDE arch/m68knommu/platform/68VZ328/de2/fixed.ld
