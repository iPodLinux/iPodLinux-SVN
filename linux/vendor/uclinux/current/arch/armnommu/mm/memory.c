#include <linux/config.h>
#include <linux/mm.h>

#ifdef MAGIC_ROM_PTR
int is_in_rom(unsigned long addr) {
	if ( FLASH_MEM_BASE <= addr && addr < (FLASH_MEM_BASE+FLASH_SIZE) )
		return 1;
	else 
		return 0;
}
#endif
