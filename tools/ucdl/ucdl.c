/* uCdl v1.0
 * A library for dynamic loading under uClinux.
 *
 * Copyright (c) 2005 Joshua Oreman. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "flat.h"
#include "elf.h"

#define printf(fmt,args...)

// Symbols loaded from a .sym file
typedef struct symbol
{
    char *sym;
    unsigned int addr;
    struct symbol *next;
} symbol;


// Relocs are linked lists because we need to iterate, not random-access.
#define RELOC_HAS_ADDEND (1<<15)   // can't be a type, since types are 8-bits
typedef struct reloc
{
    struct section *section;
    unsigned int offset, symbolidx, type, addend;
    struct esymbol *symbol;
    struct reloc *next;
} reloc;


// Array because these are random-accessed by relocs.
typedef struct esymbol
{
    const char *name;
    unsigned int nameidx, value, size, type, binding, sectionidx;
    struct section *section;
} esymbol;


// Array because these are random-accessed by symbols and relocs.
typedef struct section
{
    const char *name;
    unsigned int nameidx, type, flags, offset, size, linkidx, info, entsize, alignment;
    char *addr;
    struct section *link, *relsection;
} section;


typedef struct handle
{
    esymbol *symbols;
    section *sections;
    reloc *relocs;
    int nsecs, nsyms, shstrtabidx, symstrtabidx;
    char **strtabs;
    void *loc;
} handle;


// Used to determine the location of the text segment in memory.
void uCdl_nothing() {};

static struct symbol *mysyms;
    
// Used by modules to make sure everything is located correctly.
const int uCdl_magic __attribute__ ((section (".text"))) = 0x12345678;


/*! Initialize the uCdl library.
 * @param symfile the current running executable
 * including a symbol table.
 * @return 1 for success, 0 for error.
 */
int uCdl_init (const char *symfile) 
{
    struct symbol *cur = mysyms;
    unsigned int offset = 0;
    FILE *fp;
    struct flat_hdr header;
    long n_syms;

    if ((fp = fopen(symfile, "r")) == NULL)
	return 0;

    if (flat_read_header(fp, &header) != 0)
	return 0;

    fseek(fp, header.reloc_start + 4*header.reloc_count, SEEK_SET);

    fread(&n_syms, sizeof(long), 1, fp);
    n_syms = ntohl(n_syms);

    while (n_syms-- > 0) {
	long pos, addr = 0;
	char *sym;
	int len = 0;

	fread(&addr, sizeof(long), 1, fp);
	addr = ntohl(addr);
	pos = ftell(fp);
	while ((fgetc(fp)) != '\0')
		len++;
	fseek(fp, pos, SEEK_SET);
	sym = malloc(sizeof(char) * (len + 1));
	fread(sym, sizeof(char), len + 1, fp);
	
	printf("%s @0x%08x\n", sym, addr);

	if (strcmp(sym, "uCdl_nothing") == 0) {
	    offset = (unsigned int)&uCdl_nothing - addr;
	}

	if (cur == 0) {
	    cur = mysyms = malloc(sizeof(struct symbol));
	} else {
	    cur->next = malloc(sizeof(struct symbol));
	    cur = cur->next;
	}
	
	cur->sym = sym;
	cur->addr = addr;
	cur->next = 0;
    }

    fclose(fp);

    cur = mysyms;
    while (cur) {
	cur->addr += offset;
	if (!strcmp (cur->sym, "uCdl_magic")) {
	    if (*(int *)cur->addr != 0x12345678) {
		fprintf (stderr, "Error: incorrect offset computation:"
				" *%p (%x) != *%p (%x)  offset %x\n", cur->addr,
			       *(int *)cur->addr, &uCdl_magic, uCdl_magic,
			       offset);
		return 0;
	    }
	}
	cur = cur->next;
    }

    fprintf (stderr, "I was loaded at 0x%08x.\n", offset);
    
    return 1;
}


/*! Load a module.
 * @param path Path to the module to load. Should be an ELF 32-bit LSB relocatable
 * (.o file).
 */
void *uCdl_open (const char *path)
{
    Elf_Ehdr eh;
    Elf_Shdr sh;
    int i;
    int fd = open (path, O_RDONLY | O_EXCL);
    if (fd == -1) {
	fprintf (stderr, "Can't open %s: %s\n", path, strerror (errno));
	return 0;
    }

    read (fd, &eh, sizeof(Elf_Ehdr));
    if (!IS_ELF (eh)) {
	fprintf (stderr, "%s: Not an ELF file (%x %x %x %x)\n", path, eh.e_ident[EI_MAG0],
		 eh.e_ident[EI_MAG1], eh.e_ident[EI_MAG2], eh.e_ident[EI_MAG3]);
	return 0;
    }
    if (eh.e_ident[EI_CLASS] != ELFCLASS32) {
	fprintf (stderr, "%s: Not a 32-bit ELF file (%d)\n", path, eh.e_ident[EI_CLASS]);
	return 0;
    }
    if (eh.e_ident[EI_DATA] != ELFDATA2LSB) {
	fprintf (stderr, "%s: Not an LSB ELF file (%d)\n", path, eh.e_ident[EI_DATA]);
	return 0;
    }
    if (eh.e_type != ET_REL) {
	fprintf (stderr, "%s: Not an ELF relocatable (%d)\n", path, eh.e_ident[EI_DATA]);
	return 0;
    }
    if (eh.e_machine != EM_ARM) {
	fprintf (stderr, "%s: Not an ARM ELF file (%d)\n", path, eh.e_machine);
	return 0;
    }
    if (!eh.e_shoff) {
	fprintf (stderr, "%s: No section header table\n", path);
	return 0;
    }

    int shnum = eh.e_shnum;
    int memsize = 0;
    handle *ret = malloc (sizeof(handle));
    ret->loc = ret->symbols = 0;
    ret->strtabs = calloc (eh.e_shnum, sizeof(char*));
    ret->shstrtabidx = eh.e_shstrndx;
    ret->sections = malloc (sizeof(section) * eh.e_shnum);
    
    for (i = 0; i < eh.e_shnum; i++) ret->strtabs[i] = 0;

    section *sec = ret->sections;

    // Read section headers, figure out memory size.
    lseek (fd, eh.e_shoff, SEEK_SET);
    while (shnum) {
	read (fd, &sh, sizeof(Elf_Shdr));

	sec->nameidx = sh.sh_name;
	sec->type = sh.sh_type;
	sec->flags = sh.sh_flags;
	sec->addr = (char *)sh.sh_addr;
	sec->offset = sh.sh_offset;
	sec->size = sh.sh_size;
	sec->entsize = sh.sh_entsize;
	sec->linkidx = sh.sh_link;
	sec->info = sh.sh_info;
	sec->alignment = sh.sh_addralign;

	if (sh.sh_type == SHT_PROGBITS || sh.sh_type == SHT_NOBITS) {
	    memsize += sh.sh_size;
	}

	shnum--;
	sec++;
    }

    ret->loc = malloc (memsize);
    if (!ret->loc) {
	fprintf (stderr, "Out of memory\n");
	free (ret);
	return 0;
    }

    unsigned int lastoff = 0;

    ret->nsecs = eh.e_shnum;

    // Load sections, resolve indices -> pointers
    sec = ret->sections;
    shnum = 0;
    while (shnum < ret->nsecs) {
	if (sec->linkidx != SHN_UNDEF) {
	    sec->link = ret->sections + sec->linkidx;
	} else {
	    sec->link = 0;
	}

	switch (sec->type) {
	case SHT_PROGBITS:
	    if (!(sec->flags & SHF_ALLOC)) break;
	    
	    lseek (fd, sec->offset, SEEK_SET);
	    sec->addr = ret->loc + lastoff;

	    lastoff += sec->size;
	    if (sec->alignment > 1)
		lastoff = (lastoff + (1 << sec->alignment) - 2) & ~((1 << sec->alignment) - 1);

	    read (fd, (char *)sec->addr, sec->size);
	    break;
	case SHT_NOBITS:
	    if (!(sec->flags & SHF_ALLOC)) break;

	    sec->addr = ret->loc + lastoff;

	    lastoff += sec->size;
	    if (sec->alignment > 1)
		lastoff = (lastoff + (1 << sec->alignment) - 2) & ~((1 << sec->alignment) - 1);

	    memset (sec->addr, 0, sec->size);
	    break;
	case SHT_STRTAB:
	    ret->strtabs[shnum] = malloc (sec->size);
	    lseek (fd, sec->offset, SEEK_SET);
	    read (fd, ret->strtabs[shnum], sec->size);
	    break;
	case SHT_SYMTAB:
	    ret->nsyms = sec->size / sec->entsize;
	    ret->symstrtabidx = sec->linkidx;
	    ret->symbols = malloc (sec->size / sec->entsize * sizeof(esymbol));
	    {
		Elf_Sym st;
		esymbol *sym = ret->symbols;
		int nent = sec->size / sec->entsize;

		lseek (fd, sec->offset, SEEK_SET);

		for (i = 0; i < nent; i++, sym++) {
		    read (fd, &st, sec->entsize);
		    sym->nameidx = st.st_name;
		    sym->value = st.st_value;
		    sym->size = st.st_size;
		    sym->binding = st.st_info >> 4;
		    sym->type = st.st_info & 0xf;
		    sym->sectionidx = st.st_shndx;
		    sym->section = 0;
		}
	    }
	    break;
	case SHT_REL:
	case SHT_RELA:
	    // handle reloc linked list
	    {
		int nent = sec->size / sec->entsize;
		Elf_Rel r;
		Elf_Rela ra;
		reloc *rel = ret->relocs;
		if (rel) {
		    while (rel->next) rel = rel->next;
		}

		lseek (fd, sec->offset, SEEK_SET);
		
		for (i = 0; i < nent; i++) {
		    if (sec->type == SHT_REL) {
			read (fd, &r, sizeof(r));
		    } else {
			read (fd, &ra, sizeof(ra));
		    }

		    if (!((ret->sections + sec->info)->flags & SHF_ALLOC))
			continue;

		    if (!rel) {
			rel = ret->relocs = malloc (sizeof(reloc));
		    } else {
			rel->next = malloc (sizeof(reloc));
			rel = rel->next;
		    }

		    rel->section = ret->sections + sec->info;

		    if (sec->type == SHT_REL) {
			rel->offset = r.r_offset;
			rel->symbolidx = r.r_info >> 8;
			rel->type = r.r_info & 0xff;
			rel->addend = 0;
		    } else {
			rel->offset = ra.r_offset;
			rel->symbolidx = ra.r_info >> 8;
			rel->type = (r.r_info & 0xff) | RELOC_HAS_ADDEND;
			rel->addend = ra.r_addend;
		    }
		}
	    }
	    break;
	}

	sec++;
	shnum++;
    }

    // Resolve names for sections
    for (sec = ret->sections, i = 0; i < ret->nsecs; i++, sec++) {
	sec->name = ret->strtabs[ret->shstrtabidx] + sec->nameidx;
    }

    // Resolve names and sections for symbols
    esymbol *sym;
    for (sym = ret->symbols, i = 0; i < ret->nsyms; i++, sym++) {
	sym->section = ret->sections + sym->sectionidx;

	if (sym->type == STT_SECTION && !sym->nameidx)
	    sym->name = ret->strtabs[ret->shstrtabidx] + sym->section->nameidx;
	else
	    sym->name = ret->strtabs[ret->symstrtabidx] + sym->nameidx;
    }

    // Resolve symbols for relocations
    reloc *rel;
    for (rel = ret->relocs; rel; rel = rel->next) {
	rel->symbol = ret->symbols + rel->symbolidx;
    }

    // Print them all and return.
    // This attempts to match the output of `objdump -x'

    printf ("%s:    file format elf32-littlearm\n\n", path);
    
    
    printf ("Sections:\n%3s %-13s %-9s %-9s %-9s %4s\n",
	    "Idx", "Name", "VMA", "Size", "File off", "Algn");
    for (sec = ret->sections, i = 0; i < ret->nsecs; i++, sec++) {
	printf ("%3d %-13s %08x  %08x  %08x  2**%d\n",
		i, sec->name, sec->addr, sec->size, sec->offset, sec->alignment);
    }
    printf ("\n");
    
    printf ("SYMBOL TABLE:\n");
    for (sym = ret->symbols, i = 0; i < ret->nsyms; i++, sym++) {
	printf ("%08x %c     %c %-6s %08x %s\n",
		sym->value,
		(sym->binding == STB_LOCAL)? 'l' : (sym->binding == STB_GLOBAL)? 'g' : ' ',
		(sym->type == STT_OBJECT)? 'O' : (sym->type == STT_FUNC)? 'F' : ' ',
		(sym->sectionidx == SHN_ABS)? "*ABS*" : (sym->sectionidx == SHN_UNDEF)? "*UND*" :
		    sym->section->name,
		sym->size,
		sym->name);
    }
    
    printf ("\nRELOCATIONS:\n"
	    "%-8s %-18s %s\n",
	    "OFFSET", "TYPE", "VALUE");

    for (rel = ret->relocs; rel; rel = rel->next) {
	printf ("%08x %-18d %s\n",
		rel->offset, rel->type, rel->symbol->name);
    }
    printf ("\n");

    printf ("\nPatching up symbols...\n");
    ret->sections[0].addr = 0;
    for (sym = ret->symbols + 1, i = 1; i < ret->nsyms; i++, sym++) {
	if (sym->sectionidx == SHN_UNDEF) {
	    symbol *sy = mysyms;
	    int defined = 0;

	    sym->sectionidx = 0; /* yes, I know it's the same thing. */
	    sym->section = ret->sections /* + 0 */;

	    while (sy) {
		if (!strcmp (sy->sym, sym->name)) {
		    defined++;
		    sym->value = sy->addr;
		    sym->size = 0;
		    sym->type = STT_FUNC;
		    sym->binding = STB_GLOBAL;
		}
		sy = sy->next;
	    }

	    if (defined == 0) {
		fprintf (stderr, "%s: undefined symbol: %s\n", path, sym->name);
		exit (1);
	    }
	    if (defined > 1) {
		fprintf (stderr, "%s: multiple definition of %s (%d times)\n", path, sym->name, defined);
		exit (2);
	    }

	    printf ("%08x  %s\n", sym->value, sym->name);
	}
    }

    printf ("\nPerforming relocations...\n");
    for (rel = ret->relocs; rel; rel = rel->next) {
	char *addr = rel->section->addr + rel->offset;
	unsigned short *addr16 = (unsigned short *)addr;
	unsigned int *addr32 = (unsigned int *)addr;
	
	int A = 0;
	unsigned int At, S, P;

	S = (unsigned int)rel->symbol->section->addr + rel->symbol->value;
	P = (unsigned int)rel->section->addr + rel->offset;

	// Extract the addend
	switch (rel->type & 0xff) {
	case R_ARM_NONE: A = 0; break;
	case R_ARM_PC24:
	    At = *addr32 & 0xffffff;
	    At <<= 2;
	    if (At & (1 << 25)) {
		At |= 0xfc000000;
	    }
	    A = At;
	    break;
	case R_ARM_ABS32:
	case R_ARM_REL32:
	    A = *addr32;
	    break;
	case R_ARM_PC13:
	    A = *addr32 & 0xfff;
	    if (!(*addr32 & (1 << 23)))
		A = -A;
	    break;
	case R_ARM_ABS16:
	    A = *addr16;
	    break;
	case R_ARM_ABS12:
	    A = *addr32 & 0xfff;
	    break;
	case R_ARM_ABS8:
	    A = *addr;
	    break;
	case R_ARM_SWI24:
	    A = *addr32 & 0xffffff;
	    break;
	default:
	    fprintf (stderr, "%s: unable to handle relocation extraction for type %d (probably bug)\n",
		     path, rel->type & 0xff);
	    exit (1);
	}
	if (rel->type & RELOC_HAS_ADDEND)
	    A += rel->addend;

	// Calculate the relocation
	unsigned int val;
	switch (rel->type & 0xff) {
	case R_ARM_PC24: case R_ARM_REL32: case R_ARM_PC13:
	    val = S - P + A;
	    break;
	case R_ARM_ABS32: case R_ARM_ABS16: case R_ARM_ABS12: case R_ARM_ABS8:
	case R_ARM_SWI24: case R_ARM_THM_ABS5: case R_ARM_THM_SWI8:
	    val = S + A;
	    break;
	default:
	    fprintf (stderr, "%s: unable to handle relocation calculation for type %d (probably bug)\n",
		     path, rel->type & 0xff);
	    exit (1);
	}

	// Insert relocated value
	switch (rel->type & 0xff) {
	case R_ARM_NONE:
	    break;
	case R_ARM_PC24:
	    *addr32 &= ~0xffffff;
	    *addr32 |= ((val & 0x3ffffff) >> 2);
	    break;
	case R_ARM_ABS32:
	case R_ARM_REL32:
	    *addr32 = val;
	    break;
	case R_ARM_PC13:
	    *addr32 &= ~0xfff;
	    if (val < 0) {
		*addr32 |= (1 << 23) | (-val & 0xfff);
	    } else {
		*addr32 &= ~(1 << 23);
		*addr32 |= (val & 0xfff);
	    }
	    break;
	case R_ARM_ABS16:
	    *addr16 = (unsigned short) val;
	    break;
	case R_ARM_ABS12:
	    *addr32 &= ~0xfff;
	    *addr32 |= (val & 0xfff);
	    break;
	case R_ARM_ABS8:
	    *addr = (unsigned char) val;
	    break;
	case R_ARM_SWI24:
	    *addr32 &= ~0xffffff;
	    *addr32 |= (val & 0xffffff);
	    break;
	default:
	    fprintf (stderr, "%s: unable to handle relocation insertion for type %d (probably bug)\n",
		     path, rel->type & 0xff);
	    exit (1);
	}

	// Print some status

	if (rel->type & RELOC_HAS_ADDEND)
	    A -= rel->addend;

	printf ("Relocating (S) %08x (- (P) %08x) + (A) %08x %s+ (Ad) %08x%s: %08x. Word: %08x.\n",
		S, P, A,
		(rel->type & RELOC_HAS_ADDEND)? "" : "(",
		(rel->type & RELOC_HAS_ADDEND)? rel->addend : 0,
		(rel->type & RELOC_HAS_ADDEND)? "" : ")",
		val, *addr32);
    }

    return (void *)ret;
}

void *uCdl_sym (void *handle, const char *name) 
{
    esymbol *sym;
    struct handle *h;
    int i;

    h = (struct handle *)handle;

    for (sym = h->symbols, i = 0; i < h->nsyms; i++, sym++) {
	if (!strcmp (sym->name, name)) {
	    return sym->section->addr + sym->value;
	}
    }
    fprintf (stderr, "%p: unable to find symbol %s\n", handle, name);
    return 0;
}

void uCdl_close (void *handle) 
{
    reloc *rel;
    struct handle *h;
    int i;
    
    h = (struct handle *)handle;

    for (i = 0; i < h->nsecs; i++) {
	if (h->strtabs[i]) free (h->strtabs[i]);
    }
    free (h->strtabs);
    free (h->sections);
    free (h->loc);
    
    for (rel = h->relocs; rel; rel = rel->next) {
	reloc *next = rel->next;
	free (rel);
	rel = rel->next;
    }

    free (h);
}

#ifdef TEST
int main (int argc, char **argv) 
{
    void *handle;

    if (argc < 3) {
	fprintf (stderr, "usage: ucdl symfile objfile\n");
	return 1;
    }
    
    uCdl_init (argv[1]);
    handle = uCdl_open (argv[2]);
    
    return 0;
}
#endif
