/*
 *  linux/arch/armnommu/kernel/module.c
 *
 *  Copyright (C) 2002 Russell King.
 *  Copyright (C) 2004 Hyok S. Choi, for nommu.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Module allocation method suggested by Andi Kleen.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/elf.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/string.h>

#include <asm/pgtable.h>

#if 0
#define DEBUGP(x, args...)	printk(KERN_DEBUG x, ## args)
#else
#define DEBUGP(x, args...)
#endif


void *module_alloc(unsigned long size)
{
	return size == 0 ? NULL : vmalloc(size);
}

void module_free(struct module *module, void *region)
{
	vfree(region);
}

int module_frob_arch_sections(Elf_Ehdr *hdr,
			      Elf_Shdr *sechdrs,
			      char *secstrings,
			      struct module *mod)
{
	return 0;
}

int
apply_relocate(Elf32_Shdr *sechdrs, const char *strtab, unsigned int symindex,
	       unsigned int relsec, struct module *me)
{
	unsigned int i;
	Elf32_Rel *rel = (void *)sechdrs[relsec].sh_addr;
	Elf32_Sym *sym;
	uint32_t *location;

	DEBUGP("Applying relocate section %u to %u\n", relsec,
			sechdrs[relsec].sh_info);

	for (i = 0; i < sechdrs[relsec].sh_size / sizeof(*rel); i++) {
		s32 offset;
		/* This is where to make the change */
		location = (void *)sechdrs[sechdrs[relsec].sh_info].sh_addr
				+ rel[i].r_offset;
		/* This is the symbol it is referring to. Note that all
		  undefined symbols have been resolved. */
		sym = (Elf32_Sym *)sechdrs[symindex].sh_addr
				+ ELF32_R_SYM(rel[i].r_info);

		switch (ELF32_R_TYPE(rel[i].r_info)) {
		case R_ARM_ABS32:
			/* We add the value into the location given */
			*location += sym->st_value;
			break;
		case R_ARM_PC24:
			/* Add the value, subtract its position */
			offset = (*(u32 *)location & 0x00ffffff) << 2;
			if (offset & 0x02000000)
				offset -= 0x04000000;

			offset += sym->st_value - (uint32_t)location;
			if (offset & 3 ||
			    offset <= (s32)0xfc000000 ||
			    offset >= (s32)0x04000000) {
				printk(KERN_ERR
				       "%s: relocation out of range, section "
				       "%d reloc %d sym '%s'\n", me->name,
				       relsec, i, strtab + sym->st_name);
				return -ENOEXEC;
			}

			offset >>= 2;

			*(u32 *)location &= 0xff000000;
			*(u32 *)location |= offset & 0x00ffffff;
			break;

		default:
			printk(KERN_ERR "%s: unknown relocation: %u\n",
			       me->name, ELF32_R_TYPE(rel[i].r_info));
			return -ENOEXEC;
		}
	}
	return 0;
}

int
apply_relocate_add(Elf32_Shdr *sechdrs, const char *strtab,
		   unsigned int symindex, unsigned int relsec, struct module *module)
{
	printk(KERN_ERR "module %s: ADD RELOCATION unsupported\n",
	       module->name);
	return -ENOEXEC;
}

int
module_finalize(const Elf32_Ehdr *hdr, const Elf_Shdr *sechdrs,
		struct module *module)
{
	return 0;
}

void
module_arch_cleanup(struct module *mod)
{
}
