/*
  RAM size should now be set using the config options
  the size here should stay to the maximum possible for
  any DragonEngine board
*/

MEMORY {
	rom : ORIGIN = 0x04030000, LENGTH = 2M - 196k
	ram : ORIGIN = 0x00010000, LENGTH = 8M - 64k
}
INCLUDE arch/m68knommu/platform/68VZ328/de2/fixed.ld
