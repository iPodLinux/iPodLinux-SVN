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
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "flat.h"
#include "elf.h"

/* Comment out to enable copious debugging information: */
#define printf(args...)

#define SYM_TEXT  0
#define SYM_DATA  1
#define SYM_BSS   2
#define SYM_LOCAL 4

// Symbols loaded from the calling app
typedef struct symbol
{
    char *sym;
    unsigned int addr;
    int whence, local;
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
    unsigned int nameidx, type, flags, offset, size, linkidx, info, entsize, alignment, alignmask;
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
    const char *filename;
    struct handle *next;
} handle;

struct handle *uCdl_loaded_modules = 0;

static char errbuf[256];
static char *error = 0;

// Used to determine the location of the text segment in memory.
void uCdl_nothing() {};

static struct symbol *mysyms;
    
// Used by modules to make sure everything is located correctly.
const int uCdl_magic __attribute__ ((section (".text"))) = 0x12345678;

const int uCdl_datamagic = 0x12345678;
int uCdl_bssmagic;

/*! Initialize the uCdl library.
 * @param symfile the current running executable
 * including a symbol table.
 * @return 1 for success, 0 for error.
 */
int uCdl_init (const char *symfile) 
{
    struct symbol *cur = mysyms;
    unsigned int offset = 0, dataoff = 0, bssoff = 0;
    FILE *fp;
    struct flat_hdr header;
    long n_syms;

    uCdl_bssmagic = 0x12345678;

    if ((fp = fopen(symfile, "r")) == NULL) {
    	return 0;
    }

    if (flat_read_header(fp, &header) != 0) {
    	return 0;
    }

    fseek(fp, header.reloc_start + 4*header.reloc_count, SEEK_SET);

    fread(&n_syms, sizeof(long), 1, fp);
    n_syms = ntohl(n_syms);
    if (!n_syms) {
        error = "No symbols.";
        return 0;
    }

    while (n_syms-- > 0) {
	unsigned int pos, addr = 0;
	char *sym;
	int len = 0;
	char whence;

	fread(&addr, sizeof(unsigned int), 1, fp);
	addr = ntohl(addr);
	pos = ftell(fp);
	while ((fgetc(fp)) != '\0')
		len++;
	fseek(fp, pos, SEEK_SET);
	sym = malloc(sizeof(char) * (len + 1));
	fread(sym, sizeof(char), len + 1, fp);
	fread(&whence, sizeof(char), 1, fp);
	
	if (strcmp(sym, "uCdl_nothing") == 0) {
	    offset = (unsigned int)&uCdl_nothing - addr;
	}
	if (strcmp(sym, "uCdl_datamagic") == 0) {
		dataoff = (unsigned int)&uCdl_datamagic - addr;
	}
	if (strcmp(sym, "uCdl_bssmagic") == 0) {
		bssoff = (unsigned int)&uCdl_bssmagic - addr;
	}

	if (cur == 0) {
	    cur = mysyms = calloc(1, sizeof(struct symbol));
	} else {
	    cur->next = calloc(1, sizeof(struct symbol));
	    cur = cur->next;
	}
	
	cur->sym = sym;
	cur->addr = addr;
	cur->whence = whence & 3;
        cur->local = !!(whence & SYM_LOCAL);
	cur->next = 0;
    }

    fclose(fp);

    cur = mysyms;
    while (cur) {
    	switch (cur->whence) {
    	case SYM_TEXT:
    		cur->addr += offset;
    		break;
    	case SYM_DATA:
    		cur->addr += dataoff;
    		break;
    	case SYM_BSS:
    		cur->addr += bssoff;
    		break;
    	}
	if (!strcmp (cur->sym, "uCdl_magic") || !strcmp (cur->sym, "uCdl_datamagic") ||
	    !strcmp (cur->sym, "uCdl_bssmagic")) {
	    if (*(int *)cur->addr != 0x12345678) {
	    	const int *which;
	    	if (!strcmp (cur->sym, "uCdl_datamagic")) which = &uCdl_datamagic;
	    	if (!strcmp (cur->sym, "uCdl_bssmagic")) which = &uCdl_bssmagic;
	    	if (!strcmp (cur->sym, "uCdl_magic")) which = &uCdl_magic;
		sprintf (error = errbuf, "Error: incorrect offset computation (%s):"
				" *%p (%x) != *%p (%x)  offset %x", cur->sym, cur->addr,
			       *(int *)cur->addr, which, *which,
			       offset);
		return 0;
	    }
	}
	cur = cur->next;
    }

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
	sprintf (error = errbuf, "Can't open %s: %s", path, strerror (errno));
	return 0;
    }

    read (fd, &eh, sizeof(Elf_Ehdr));
    if (!IS_ELF (eh)) {
	sprintf (error = errbuf, "Not an ELF file (%x %x %x %x)", eh.e_ident[EI_MAG0],
		 eh.e_ident[EI_MAG1], eh.e_ident[EI_MAG2], eh.e_ident[EI_MAG3]);
	return 0;
    }
    if (eh.e_ident[EI_CLASS] != ELFCLASS32) {
	sprintf (error = errbuf, "Not a 32-bit ELF file (%d)", eh.e_ident[EI_CLASS]);
	return 0;
    }
    if (eh.e_ident[EI_DATA] != ELFDATA2LSB) {
	sprintf (error = errbuf, "Not an LSB ELF file (%d)", eh.e_ident[EI_DATA]);
	return 0;
    }
    if (eh.e_type != ET_REL) {
	sprintf (error = errbuf, "Not an ELF relocatable (%d)", eh.e_ident[EI_DATA]);
	return 0;
    }
    if (eh.e_machine != EM_ARM) {
	sprintf (error = errbuf, "Not an ARM ELF file (%d)", eh.e_machine);
	return 0;
    }
    if (!eh.e_shoff) {
	error = "No section header table";
	return 0;
    }

    int shnum = eh.e_shnum;
    int memsize = 0;
    handle *ret = calloc (1, sizeof(handle));
    ret->loc = ret->symbols = 0;
    ret->filename = path;
    ret->strtabs = calloc (eh.e_shnum, sizeof(char*));
    ret->shstrtabidx = eh.e_shstrndx;
    ret->sections = calloc (eh.e_shnum, sizeof(section));
    
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
        sec->alignmask = (1 << sec->alignment) - 1;

	if (sh.sh_type == SHT_PROGBITS || sh.sh_type == SHT_NOBITS) {
	    memsize += sh.sh_size;
            if (memsize & sec->alignmask) {
                memsize = (memsize + sec->alignmask) & ~sec->alignmask;
            }
	}

	shnum--;
	sec++;
    }

    ret->loc = malloc (memsize);
    if (!ret->loc) {
	error = "Out of memory";
	free (ret);
	return 0;
    }
    memset (ret->loc, memsize, 42);

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
	    
            if (lastoff & sec->alignmask) {
                lastoff = (lastoff + sec->alignmask) & ~sec->alignmask;
            }

	    lseek (fd, sec->offset, SEEK_SET);
	    sec->addr = ret->loc + lastoff;

	    lastoff += sec->size;

	    read (fd, (char *)sec->addr, sec->size);
	    break;
	case SHT_NOBITS:
	    if (!(sec->flags & SHF_ALLOC)) break;

            if (lastoff & sec->alignmask) {
                lastoff = (lastoff + sec->alignmask) & ~sec->alignmask;
            }

	    sec->addr = ret->loc + lastoff;

	    lastoff += sec->size;

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
	    ret->symbols = calloc (sec->size / sec->entsize, sizeof(esymbol));
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
			rel = ret->relocs = calloc (1, sizeof(reloc));
		    } else {
			rel->next = calloc (1, sizeof(reloc));
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
			rel->type = (ra.r_info & 0xff) | RELOC_HAS_ADDEND;
			rel->addend = ra.r_addend;
		    }
		    rel->next = 0;
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
	    handle *curh = uCdl_loaded_modules;
	    int defined = 0;

	    sym->sectionidx = 0; /* yes, I know it's the same thing. */
	    sym->section = ret->sections /* + 0 */;

	    while (sy) {
		if (!strcmp (sy->sym, sym->name) && !sy->local) {
		    defined++;
		    sym->value = sy->addr;
		    sym->size = 0;
		    sym->type = STT_FUNC;
		    sym->binding = STB_GLOBAL;
		}
		sy = sy->next;
	    }

	    while (!defined && curh) {
		esymbol *esy;
		int i;
                int lastweak = 0;

		for (esy = curh->symbols, i = 0; i < curh->nsyms; i++, esy++) {
		    if (!strcmp (esy->name, sym->name) && (esy->binding == STB_GLOBAL || esy->binding == STB_WEAK)) {
                        int thisweak = (esy->binding == STB_WEAK);
                        if (defined && thisweak && !lastweak)
                            continue;
                        if (defined && !thisweak && lastweak)
                            defined = 0; // no multiple definition error
			defined++;
			sym->value = (unsigned int)esy->section->addr + esy->value;
			sym->size = 0;
			sym->type = STT_FUNC;
			sym->binding = STB_GLOBAL;
                        lastweak = thisweak;
		    }
		}

		curh = curh->next;
	    }

	    if (defined == 0) {
		sprintf (error = errbuf, "%s: undefined symbol: %s", path, sym->name);
		return 0;
	    }
	    if (defined > 1) {
		sprintf (error = errbuf, "%s: multiple definition of %s (%d times)", path, sym->name, defined);
		return 0;
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
            if ((unsigned long)addr & 3) {
                sprintf (error = errbuf, "Unable to handle ABS32 relocation for unaligned address %p. Relocation is at (P) %x for (S) %x, section %s, symbol %s.", addr, P, S, rel->section->name, rel->symbol->name);
                return 0;
            }
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
	    sprintf (error = errbuf, "%s: unable to handle relocation extraction for type %d (probably bug)",
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
	    sprintf (error = errbuf, "%s: unable to handle relocation calculation for type %d (probably bug)",
		     path, rel->type & 0xff);
	    exit (1);
	}

	// Insert relocated value
	switch (rel->type & 0xff) {
	case R_ARM_NONE:
	    break;
	case R_ARM_PC24:
	    *addr32 &= ~0xffffff;
	    *addr32 |= ((val >> 2) & 0xffffff);
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
	    sprintf (error = errbuf, "%s: unable to handle relocation insertion for type %d (probably bug)",
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

    handle *curh = uCdl_loaded_modules;
    ret->next = 0;
    if (!curh) {
	curh = uCdl_loaded_modules = ret;
    } else {
	while (curh->next) curh = curh->next;
	curh->next = ret;
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
    sprintf (error = errbuf, "%p: unable to find symbol %s", handle, name);
    return 0;
}

// Note: There's nothing preventing you from unloading a module
// with symbols used by another module right now. It's up to the
// app to prevent that from happening.
void uCdl_close (void *handle) 
{
    reloc *rel;
    struct handle *h, *prev;
    int i;
    
    h = (struct handle *)handle;
    if (h == uCdl_loaded_modules) {
	prev = 0;
    } else {
	prev = uCdl_loaded_modules;
	while (prev && prev->next && (prev->next != h))
	    prev = prev->next;
	
	if (!prev || !prev->next) { // off the end
	    sprintf (error = errbuf, "Error: handle %%%p is not loaded.", handle);
	    return;
	}
    }

    for (i = 0; i < h->nsecs; i++) {
	if (h->strtabs[i]) free (h->strtabs[i]);
    }
    free (h->strtabs);
    free (h->sections);
    free (h->loc);
    
    for (rel = h->relocs; rel; ) {
	reloc *next = rel->next;
	free (rel);
	rel = next;
    }

    if (h == uCdl_loaded_modules) {
	uCdl_loaded_modules = h->next;
    } else if (prev) {
	prev->next = h->next;
    }

    free (h);
}

const char *uCdl_error() 
{
    const char *ret = error;
    error = 0;
    return ret;
}

const char *uCdl_resolve_addr (unsigned long addr, unsigned long *off, const char **modname) 
{
    unsigned long closest_off = 0xffffffff;
    const char *ret = 0;
    handle *curh = uCdl_loaded_modules;
    symbol *sy = mysyms;
    
    while (sy) {
        if ((addr >= sy->addr) && (addr - sy->addr < closest_off) && sy->sym[0] != '$') {
            closest_off = addr - sy->addr;
            ret = sy->sym;
            if (modname) *modname = "<core>";
        }
        sy = sy->next;
    }

    while (curh) {
        esymbol *esy;
        int i;
        
        for (esy = curh->symbols, i = 0; i < curh->nsyms; i++, esy++) {
            unsigned long symaddr = (unsigned long)esy->section->addr + esy->value;
            if ((addr >= symaddr) && (addr - symaddr < closest_off) && esy->name[0] != '$') {
                closest_off = addr - symaddr;
                ret = esy->name;
                if (modname) *modname = curh->filename;
            }
        }
        
        curh = curh->next;
    }

    if (!ret) {
        ret = "ABS";
        closest_off = addr;
    }
    if (off) *off = closest_off;
    return ret;
}

#ifdef TEST
int main (int argc, char **argv) 
{
    void *handle;

    if (argc < 3) {
	fprintf (stderr, "usage: ucdl objfile");
	return 1;
    }
    
    uCdl_init (argv[0]);
    handle = uCdl_open (argv[1]);
    
    return 0;
}
#endif
