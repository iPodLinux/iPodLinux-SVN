// These definitions are copied from FreeBSD 5-STABLE
// as of 2004-12-20.

/////////////////////////////////////////////////////////////
//////////////////////// Common defs ////////////////////////
/////////////////////////////////////////////////////////////
///// (from FreeBSD sys/elf_common.h) ///////////////////////

/*-
 * Copyright (c) 1996-1998 John D. Polstra.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/sys/elf_common.h,v 1.10 2002/05/30 08:32:18 dfr Exp $
 */

typedef struct {
        unsigned int       n_namesz;       /* Length of name. */
        unsigned int       n_descsz;       /* Length of descriptor. */
        unsigned int       n_type;         /* Type of this note. */
} Elf_Note;

/* Indexes into the e_ident array.  Keep synced with 
   http://www.sco.com/developer/gabi/ch4.eheader.html */
#define EI_MAG0         0       /* Magic number, byte 0. */
#define EI_MAG1         1       /* Magic number, byte 1. */
#define EI_MAG2         2       /* Magic number, byte 2. */
#define EI_MAG3         3       /* Magic number, byte 3. */
#define EI_CLASS        4       /* Class of machine. */
#define EI_DATA         5       /* Data format. */
#define EI_VERSION      6       /* ELF format version. */
#define EI_OSABI        7       /* Operating system / ABI identification */
#define EI_ABIVERSION   8       /* ABI version */
#define OLD_EI_BRAND    8       /* Start of architecture identification. */
#define EI_PAD          9       /* Start of padding (per SVR4 ABI). */
#define EI_NIDENT       16      /* Size of e_ident array. */

/* Values for the magic number bytes. */
#define ELFMAG0         0x7f
#define ELFMAG1         'E'
#define ELFMAG2         'L'
#define ELFMAG3         'F'
#define ELFMAG          "\177ELF"       /* magic string */
#define SELFMAG         4               /* magic string size */

/* Values for e_ident[EI_VERSION] and e_version. */
#define EV_NONE         0
#define EV_CURRENT      1

/* Values for e_ident[EI_CLASS]. */
#define ELFCLASSNONE    0       /* Unknown class. */
#define ELFCLASS32      1       /* 32-bit architecture. */
#define ELFCLASS64      2       /* 64-bit architecture. */

/* Values for e_ident[EI_DATA]. */
#define ELFDATANONE     0       /* Unknown data format. */
#define ELFDATA2LSB     1       /* 2's complement little-endian. */
#define ELFDATA2MSB     2       /* 2's complement big-endian. */

/* Values for e_ident[EI_OSABI]. */
#define ELFOSABI_SYSV           0       /* UNIX System V ABI */
#define ELFOSABI_NONE           ELFOSABI_SYSV   /* symbol used in old spec */
#define ELFOSABI_HPUX           1       /* HP-UX operating system */
#define ELFOSABI_NETBSD         2       /* NetBSD */
#define ELFOSABI_LINUX          3       /* GNU/Linux */
#define ELFOSABI_HURD           4       /* GNU/Hurd */
#define ELFOSABI_86OPEN         5       /* 86Open common IA32 ABI */
#define ELFOSABI_SOLARIS        6       /* Solaris */
#define ELFOSABI_MONTEREY       7       /* Monterey */
#define ELFOSABI_IRIX           8       /* IRIX */
#define ELFOSABI_FREEBSD        9       /* FreeBSD */
#define ELFOSABI_TRU64          10      /* TRU64 UNIX */
#define ELFOSABI_MODESTO        11      /* Novell Modesto */
#define ELFOSABI_OPENBSD        12      /* OpenBSD */
#define ELFOSABI_ARM            97      /* ARM */
#define ELFOSABI_STANDALONE     255     /* Standalone (embedded) application */

/* e_ident */
#define IS_ELF(ehdr)    ((ehdr).e_ident[EI_MAG0] == ELFMAG0 && \
                         (ehdr).e_ident[EI_MAG1] == ELFMAG1 && \
                         (ehdr).e_ident[EI_MAG2] == ELFMAG2 && \
                         (ehdr).e_ident[EI_MAG3] == ELFMAG3)

/* Values for e_type. */
#define ET_NONE         0       /* Unknown type. */
#define ET_REL          1       /* Relocatable. */
#define ET_EXEC         2       /* Executable. */
#define ET_DYN          3       /* Shared object. */
#define ET_CORE         4       /* Core file. */

/* Values for e_machine. */
#define EM_NONE         0       /* Unknown machine. */
#define EM_M32          1       /* AT&T WE32100. */
#define EM_SPARC        2       /* Sun SPARC. */
#define EM_386          3       /* Intel i386. */
#define EM_68K          4       /* Motorola 68000. */
#define EM_88K          5       /* Motorola 88000. */
#define EM_486          6       /* Intel i486. */
#define EM_860          7       /* Intel i860. */
#define EM_MIPS         8       /* MIPS R3000 Big-Endian only */

/* Extensions.  This list is not complete. */
#define EM_S370         9       /* IBM System/370 */
#define EM_MIPS_RS4_BE  10      /* MIPS R4000 Big-Endian */ /* Depreciated */
#define EM_PARISC       15      /* HPPA */
#define EM_SPARC32PLUS  18      /* SPARC v8plus */
#define EM_PPC          20      /* PowerPC 32-bit */
#define EM_PPC64        21      /* PowerPC 64-bit */
#define EM_ARM          40      /* ARM */
#define EM_SPARCV9      43      /* SPARC v9 64-bit */
#define EM_IA_64        50      /* Intel IA-64 Processor */
#define EM_X86_64       62      /* Advanced Micro Devices x86-64 */
#define EM_ALPHA        0x9026  /* Alpha (written in the absence of an ABI */

/* Special section indexes. */
#define SHN_UNDEF            0          /* Undefined, missing, irrelevant. */
#define SHN_LORESERVE   0xff00          /* First of reserved range. */
#define SHN_LOPROC      0xff00          /* First processor-specific. */
#define SHN_HIPROC      0xff1f          /* Last processor-specific. */
#define SHN_ABS         0xfff1          /* Absolute values. */
#define SHN_COMMON      0xfff2          /* Common data. */
#define SHN_HIRESERVE   0xffff          /* Last of reserved range. */

/* sh_type */
#define SHT_NULL        0               /* inactive */
#define SHT_PROGBITS    1               /* program defined information */
#define SHT_SYMTAB      2               /* symbol table section */
#define SHT_STRTAB      3               /* string table section */
#define SHT_RELA        4               /* relocation section with addends */
#define SHT_HASH        5               /* symbol hash table section */
#define SHT_DYNAMIC     6               /* dynamic section */ 
#define SHT_NOTE        7               /* note section */
#define SHT_NOBITS      8               /* no space section */
#define SHT_REL         9               /* relocation section - no addends */
#define SHT_SHLIB       10              /* reserved - purpose unknown */
#define SHT_DYNSYM      11              /* dynamic symbol table section */ 
#define SHT_NUM         12              /* number of section types */
#define SHT_LOOS        0x60000000      /* First of OS specific semantics */
#define SHT_HIOS        0x6fffffff      /* Last of OS specific semantics */
#define SHT_LOPROC      0x70000000      /* reserved range for processor */
#define SHT_HIPROC      0x7fffffff      /* specific section header types */
#define SHT_LOUSER      0x80000000      /* reserved range for application */
#define SHT_HIUSER      0xffffffff      /* specific indexes */

/* Flags for sh_flags. */
#define SHF_WRITE       0x1             /* Section contains writable data. */
#define SHF_ALLOC       0x2             /* Section occupies memory. */
#define SHF_EXECINSTR   0x4             /* Section contains instructions. */
#define SHF_TLS         0x400           /* Section contains TLS data. */
#define SHF_MASKPROC    0xf0000000      /* Reserved for processor-specific. */

/* Values for p_type. */
#define PT_NULL         0       /* Unused entry. */
#define PT_LOAD         1       /* Loadable segment. */
#define PT_DYNAMIC      2       /* Dynamic linking information segment. */
#define PT_INTERP       3       /* Pathname of interpreter. */
#define PT_NOTE         4       /* Auxiliary information. */
#define PT_SHLIB        5       /* Reserved (not used). */
#define PT_PHDR         6       /* Location of program header itself. */
#define PT_TLS          7       /* Thread local storage segment */

#define PT_COUNT        8       /* Number of defined p_type values. */

#define PT_LOOS         0x60000000      /* OS-specific */
#define PT_HIOS         0x6fffffff      /* OS-specific */
#define PT_LOPROC       0x70000000      /* First processor-specific type. */
#define PT_HIPROC       0x7fffffff      /* Last processor-specific type. */

/* Values for p_flags. */
#define PF_X            0x1     /* Executable. */
#define PF_W            0x2     /* Writable. */
#define PF_R            0x4     /* Readable. */

/* Values for d_tag. */
#define DT_NULL         0       /* Terminating entry. */
#define DT_NEEDED       1       /* String table offset of a needed shared
                                   library. */
#define DT_PLTRELSZ     2       /* Total size in bytes of PLT relocations. */
#define DT_PLTGOT       3       /* Processor-dependent address. */
#define DT_HASH         4       /* Address of symbol hash table. */
#define DT_STRTAB       5       /* Address of string table. */
#define DT_SYMTAB       6       /* Address of symbol table. */
#define DT_RELA         7       /* Address of ElfNN_Rela relocations. */
#define DT_RELASZ       8       /* Total size of ElfNN_Rela relocations. */
#define DT_RELAENT      9       /* Size of each ElfNN_Rela relocation entry. */
#define DT_STRSZ        10      /* Size of string table. */
#define DT_SYMENT       11      /* Size of each symbol table entry. */
#define DT_INIT         12      /* Address of initialization function. */
#define DT_FINI         13      /* Address of finalization function. */
#define DT_SONAME       14      /* String table offset of shared object
                                   name. */
#define DT_RPATH        15      /* String table offset of library path. [sup] */
#define DT_SYMBOLIC     16      /* Indicates "symbolic" linking. [sup] */
#define DT_REL          17      /* Address of ElfNN_Rel relocations. */
#define DT_RELSZ        18      /* Total size of ElfNN_Rel relocations. */
#define DT_RELENT       19      /* Size of each ElfNN_Rel relocation. */
#define DT_PLTREL       20      /* Type of relocation used for PLT. */
#define DT_DEBUG        21      /* Reserved (not used). */
#define DT_TEXTREL      22      /* Indicates there may be relocations in
                                   non-writable segments. [sup] */
#define DT_JMPREL       23      /* Address of PLT relocations. */
#define DT_BIND_NOW     24      /* [sup] */
#define DT_INIT_ARRAY   25      /* Address of the array of pointers to
                                   initialization functions */
#define DT_FINI_ARRAY   26      /* Address of the array of pointers to
                                   termination functions */
#define DT_INIT_ARRAYSZ 27      /* Size in bytes of the array of
                                   initialization functions. */
#define DT_FINI_ARRAYSZ 28      /* Size in bytes of the array of
                                   terminationfunctions. */
#define DT_RUNPATH      29      /* String table offset of a null-terminated
                                   library search path string. */
#define DT_FLAGS        30      /* Object specific flag values. */
#define DT_ENCODING     32      /* Values greater than or equal to DT_ENCODING
                                   and less than DT_LOOS follow the rules for
                                   the interpretation of the d_un union
                                   as follows: even == 'd_ptr', even == 'd_val'
                                   or none */
#define DT_PREINIT_ARRAY 32     /* Address of the array of pointers to
                                   pre-initialization functions. */
#define DT_PREINIT_ARRAYSZ 33   /* Size in bytes of the array of
                                   pre-initialization functions. */

#define DT_COUNT        33      /* Number of defined d_tag values. */

#define DT_LOOS         0x6000000d      /* First OS-specific */
#define DT_HIOS         0x6fff0000      /* Last OS-specific */
#define DT_LOPROC       0x70000000      /* First processor-specific type. */
#define DT_HIPROC       0x7fffffff      /* Last processor-specific type. */

/* Values for DT_FLAGS */
#define DF_ORIGIN       0x0001  /* Indicates that the object being loaded may
                                   make reference to the $ORIGIN substitution
                                   string */
#define DF_SYMBOLIC     0x0002  /* Indicates "symbolic" linking. */
#define DF_TEXTREL      0x0004  /* Indicates there may be relocations in
                                   non-writable segments. */
#define DF_BIND_NOW     0x0008  /* Indicates that the dynamic linker should
                                   process all relocations for the object
                                   containing this entry before transferring
                                   control to the program. */
#define DF_STATIC_TLS   0x0010  /* Indicates that the shared object or
                                   executable contains code using a static
                                   thread-local storage scheme. */

/* Values for n_type.  Used in core files. */
#define NT_PRSTATUS     1       /* Process status. */
#define NT_FPREGSET     2       /* Floating point registers. */
#define NT_PRPSINFO     3       /* Process state info. */

/* Symbol Binding - ELFNN_ST_BIND - st_info */
#define STB_LOCAL       0       /* Local symbol */
#define STB_GLOBAL      1       /* Global symbol */
#define STB_WEAK        2       /* like global - lower precedence */
#define STB_LOPROC      13      /* reserved range for processor */
#define STB_HIPROC      15      /*  specific symbol bindings */

/* Symbol type - ELFNN_ST_TYPE - st_info */
#define STT_NOTYPE      0       /* Unspecified type. */
#define STT_OBJECT      1       /* Data object. */
#define STT_FUNC        2       /* Function. */
#define STT_SECTION     3       /* Section. */
#define STT_FILE        4       /* Source file. */
#define STT_TLS         6       /* TLS object. */
#define STT_LOPROC      13      /* reserved range for processor */
#define STT_HIPROC      15      /*  specific symbol types */

/* Special symbol table indexes. */
#define STN_UNDEF       0       /* Undefined symbol index. */

/////////////////////////////////////////////////////////////
//////////////////////// 32-bit defs ////////////////////////
/////////////////////////////////////////////////////////////
///// (from FreeBSD sys/elf32.h) ////////////////////////////

/*-
 * Copyright (c) 1996-1998 John D. Polstra.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/sys/elf32.h,v 1.10 2002/05/30 08:32:18 dfr Exp $
 */

typedef unsigned int       Elf_Addr;
typedef unsigned short       Elf_Half;
typedef unsigned int       Elf_Off;
typedef int         Elf_Sword;
typedef unsigned int       Elf_Word;
typedef unsigned int       Elf_Size;
typedef Elf_Off       Elf_Hashelt;

/*
 * ELF header.
 */

typedef struct {
        unsigned char   e_ident[EI_NIDENT];     /* File identification. */
        Elf_Half      e_type;         /* File type. */
        Elf_Half      e_machine;      /* Machine architecture. */
        Elf_Word      e_version;      /* ELF format version. */
        Elf_Addr      e_entry;        /* Entry point. */
        Elf_Off       e_phoff;        /* Program header file offset. */
        Elf_Off       e_shoff;        /* Section header file offset. */
        Elf_Word      e_flags;        /* Architecture-specific flags. */
        Elf_Half      e_ehsize;       /* Size of ELF header in bytes. */
        Elf_Half      e_phentsize;    /* Size of program header entry. */
        Elf_Half      e_phnum;        /* Number of program header entries. */
        Elf_Half      e_shentsize;    /* Size of section header entry. */
        Elf_Half      e_shnum;        /* Number of section header entries. */
        Elf_Half      e_shstrndx;     /* Section name strings section. */
} Elf_Ehdr;

/*
 * Section header.
 */

typedef struct {
        Elf_Word      sh_name;        /* Section name (index into the
                                           section header string table). */
        Elf_Word      sh_type;        /* Section type. */
        Elf_Word      sh_flags;       /* Section flags. */
        Elf_Addr      sh_addr;        /* Address in memory image. */
        Elf_Off       sh_offset;      /* Offset in file. */
        Elf_Size      sh_size;        /* Size in bytes. */
        Elf_Word      sh_link;        /* Index of a related section. */
        Elf_Word      sh_info;        /* Depends on section type. */
        Elf_Size      sh_addralign;   /* Alignment in bytes. */
        Elf_Size      sh_entsize;     /* Size of each entry in section. */
} Elf_Shdr;

/*
 * Program header.
 */

typedef struct {
        Elf_Word      p_type;         /* Entry type. */
        Elf_Off       p_offset;       /* File offset of contents. */
        Elf_Addr      p_vaddr;        /* Virtual address in memory image. */
        Elf_Addr      p_paddr;        /* Physical address (not used). */
        Elf_Size      p_filesz;       /* Size of contents in file. */
        Elf_Size      p_memsz;        /* Size of contents in memory. */
        Elf_Word      p_flags;        /* Access permission flags. */
        Elf_Size      p_align;        /* Alignment in memory and file. */
} Elf_Phdr;

/*
 * Dynamic structure.  The ".dynamic" section contains an array of them.
 */

typedef struct {
        Elf_Sword     d_tag;          /* Entry type. */
        union {
                Elf_Size      d_val;  /* Integer value. */
                Elf_Addr      d_ptr;  /* Address value. */
        } d_un;
} Elf_Dyn;

/*
 * Relocation entries.
 */

/* Relocations that don't need an addend field. */
typedef struct {
        Elf_Addr      r_offset;       /* Location to be relocated. */
        Elf_Word      r_info;         /* Relocation type and symbol index. */
} Elf_Rel;

/* Relocations that need an addend field. */
typedef struct {
        Elf_Addr      r_offset;       /* Location to be relocated. */
        Elf_Word      r_info;         /* Relocation type and symbol index. */
        Elf_Sword     r_addend;       /* Addend. */
} Elf_Rela;

/* Macros for accessing the fields of r_info. */
#define ELF_R_SYM(info)       ((info) >> 8)
#define ELF_R_TYPE(info)      ((unsigned char)(info))

/* Macro for constructing r_info from field values. */
#define ELF_R_INFO(sym, type) (((sym) << 8) + (unsigned char)(type))

/*
 * Symbol table entries.
 */

typedef struct {
        Elf_Word      st_name;        /* String table index of name. */
        Elf_Addr      st_value;       /* Symbol value. */
        Elf_Size      st_size;        /* Size of associated object. */
        unsigned char   st_info;        /* Type and binding information. */
        unsigned char   st_other;       /* Reserved (not used). */
        Elf_Half      st_shndx;       /* Section index of symbol. */
} Elf_Sym;

////////////////////////////////////////////////////////////////
//////////////////////////////// From BFD from binutils 2.14 ///
////////////////////////////////////////////////////////////////

/////// elf/common.h ///////

/* Macros for accessing the fields of st_info. */
#define ELF_ST_BIND(info)             ((info) >> 4)
#define ELF_ST_TYPE(info)             ((info) & 0xf)

/* Macro for constructing st_info from field values. */
#define ELF_ST_INFO(bind, type)       (((bind) << 4) + ((type) & 0xf))

#ifdef RELOC_MACROS_GEN_FUNC

/* This function takes the relocation number and returns the
   string version name of the name of that relocation.  If
   the relocation is not recognised, NULL is returned.  */

#define START_RELOC_NUMBERS(name)   				\
static const char * name    (unsigned long rtype); 	\
static const char *						\
name (rtype)							\
	unsigned long rtype;					\
{								\
  switch (rtype)						\
  {

#if defined (__STDC__) || defined (ALMOST_STDC)
#define RELOC_NUMBER(name, number)  case number : return #name ;
#else
#define RELOC_NUMBER(name, number)  case number : return "name" ;
#endif

#define FAKE_RELOC(name, number)
#define EMPTY_RELOC(name)

#define END_RELOC_NUMBERS(name)	\
    default: return NULL;	\
  }				\
}


#else /* Default to generating enum.  */

#define START_RELOC_NUMBERS(name)   enum name {
#define RELOC_NUMBER(name, number)  name = number,
#define FAKE_RELOC(name, number)    name = number,
#define EMPTY_RELOC(name)           name,
#define END_RELOC_NUMBERS(name)     name };

#endif

/////// elf/arm.h ///////
/* Processor specific flags for the ELF header e_flags field.  */
#define EF_ARM_RELEXEC     0x01
#define EF_ARM_HASENTRY    0x02
#define EF_ARM_INTERWORK   0x04
#define EF_ARM_APCS_26     0x08
#define EF_ARM_APCS_FLOAT  0x10
#define EF_ARM_PIC         0x20
#define EF_ARM_ALIGN8      0x40         /* 8-bit structure alignment is in use.  */
#define EF_ARM_NEW_ABI     0x80
#define EF_ARM_OLD_ABI     0x100
#define EF_ARM_SOFT_FLOAT  0x200
#define EF_ARM_VFP_FLOAT   0x400
#define EF_ARM_MAVERICK_FLOAT 0x800

/* Other constants defined in the ARM ELF spec. version B-01.  */
#define EF_ARM_SYMSARESORTED 0x04       /* NB conflicts with EF_INTERWORK */
#define EF_ARM_DYNSYMSUSESEGIDX 0x08    /* NB conflicts with EF_APCS26 */
#define EF_ARM_MAPSYMSFIRST 0x10        /* NB conflicts with EF_APCS_FLOAT */
#define EF_ARM_EABIMASK      0xFF000000

#define EF_ARM_EABI_VERSION(flags) ((flags) & EF_ARM_EABIMASK)
#define EF_ARM_EABI_UNKNOWN  0x00000000
#define EF_ARM_EABI_VER1     0x01000000
#define EF_ARM_EABI_VER2     0x02000000

/* Local aliases for some flags to match names used by COFF port.  */
#define F_INTERWORK        EF_ARM_INTERWORK
#define F_APCS26           EF_ARM_APCS_26
#define F_APCS_FLOAT       EF_ARM_APCS_FLOAT
#define F_PIC              EF_ARM_PIC
#define F_SOFT_FLOAT       EF_ARM_SOFT_FLOAT
#define F_VFP_FLOAT        EF_ARM_VFP_FLOAT

/* Additional symbol types for Thumb.  */
#define STT_ARM_TFUNC      STT_LOPROC   /* A Thumb function.  */
#define STT_ARM_16BIT      STT_HIPROC   /* A Thumb label.  */

/* ARM-specific values for sh_flags.  */
#define SHF_ENTRYSECT      0x10000000   /* Section contains an entry point.  */
#define SHF_COMDEF         0x80000000   /* Section may be multiply defined in the input to a link step.  */

/* ARM-specific program header flags.  */
#define PF_ARM_SB          0x10000000   /* Segment contains the location addressed by the static base.  */
#define PF_ARM_PI          0x20000000   /* Segment is position-independent.  */
#define PF_ARM_ABS         0x40000000   /* Segment must be loaded at its base address.  */

/* Relocation types.  */

START_RELOC_NUMBERS (elf_arm_reloc_type)
  RELOC_NUMBER (R_ARM_NONE,             0)
  RELOC_NUMBER (R_ARM_PC24,             1)
  RELOC_NUMBER (R_ARM_ABS32,            2)
  RELOC_NUMBER (R_ARM_REL32,            3)
#ifdef OLD_ARM_ABI
  RELOC_NUMBER (R_ARM_ABS8,             4)
  RELOC_NUMBER (R_ARM_ABS16,            5)
  RELOC_NUMBER (R_ARM_ABS12,            6)
  RELOC_NUMBER (R_ARM_THM_ABS5,         7)
  RELOC_NUMBER (R_ARM_THM_PC22,         8)
  RELOC_NUMBER (R_ARM_SBREL32,          9)
  RELOC_NUMBER (R_ARM_AMP_VCALL9,      10)
  RELOC_NUMBER (R_ARM_THM_PC11,        11)   /* Cygnus extension to abi: Thumb unconditional branch.  */
  RELOC_NUMBER (R_ARM_THM_PC9,         12)   /* Cygnus extension to abi: Thumb conditional branch.  */
  RELOC_NUMBER (R_ARM_GNU_VTINHERIT,   13)
  RELOC_NUMBER (R_ARM_GNU_VTENTRY,     14)
#else /* not OLD_ARM_ABI */
  RELOC_NUMBER (R_ARM_PC13,             4)
  RELOC_NUMBER (R_ARM_ABS16,            5)
  RELOC_NUMBER (R_ARM_ABS12,            6)
  RELOC_NUMBER (R_ARM_THM_ABS5,         7)
  RELOC_NUMBER (R_ARM_ABS8,             8)
  RELOC_NUMBER (R_ARM_SBREL32,          9)
  RELOC_NUMBER (R_ARM_THM_PC22,        10)
  RELOC_NUMBER (R_ARM_THM_PC8,         11)
  RELOC_NUMBER (R_ARM_AMP_VCALL9,      12)
  RELOC_NUMBER (R_ARM_SWI24,           13)
  RELOC_NUMBER (R_ARM_THM_SWI8,        14)
  RELOC_NUMBER (R_ARM_XPC25,           15)
  RELOC_NUMBER (R_ARM_THM_XPC22,       16)
#endif /* not OLD_ARM_ABI */
  RELOC_NUMBER (R_ARM_COPY,            20)   /* Copy symbol at runtime.  */
  RELOC_NUMBER (R_ARM_GLOB_DAT,        21)   /* Create GOT entry.  */
  RELOC_NUMBER (R_ARM_JUMP_SLOT,       22)   /* Create PLT entry.  */
  RELOC_NUMBER (R_ARM_RELATIVE,        23)   /* Adjust by program base.  */
  RELOC_NUMBER (R_ARM_GOTOFF,          24)   /* 32 bit offset to GOT.  */
  RELOC_NUMBER (R_ARM_GOTPC,           25)   /* 32 bit PC relative offset to GOT.  */
  RELOC_NUMBER (R_ARM_GOT32,           26)   /* 32 bit GOT entry.  */
  RELOC_NUMBER (R_ARM_PLT32,           27)   /* 32 bit PLT address.  */
#ifdef OLD_ARM_ABI
  FAKE_RELOC   (FIRST_INVALID_RELOC,   28)
  FAKE_RELOC   (LAST_INVALID_RELOC,   249)
#else /* not OLD_ARM_ABI */
  FAKE_RELOC   (FIRST_INVALID_RELOC1,  28)
  FAKE_RELOC   (LAST_INVALID_RELOC1,   31)
  RELOC_NUMBER (R_ARM_ALU_PCREL7_0,    32)
  RELOC_NUMBER (R_ARM_ALU_PCREL15_8,   33)
  RELOC_NUMBER (R_ARM_ALU_PCREL23_15,  34)
  RELOC_NUMBER (R_ARM_LDR_SBREL11_0,   35)
  RELOC_NUMBER (R_ARM_ALU_SBREL19_12,  36)
  RELOC_NUMBER (R_ARM_ALU_SBREL27_20,  37)
  FAKE_RELOC   (FIRST_INVALID_RELOC2,  38)
  FAKE_RELOC   (LAST_INVALID_RELOC2,   99)
  RELOC_NUMBER (R_ARM_GNU_VTENTRY,    100)
  RELOC_NUMBER (R_ARM_GNU_VTINHERIT,  101)
  RELOC_NUMBER (R_ARM_THM_PC11,       102)   /* Cygnus extension to abi: Thumb unconditional branch.  */
  RELOC_NUMBER (R_ARM_THM_PC9,        103)   /* Cygnus extension to abi: Thumb conditional branch.  */
  FAKE_RELOC   (FIRST_INVALID_RELOC3, 104)
  FAKE_RELOC   (LAST_INVALID_RELOC3,  248)
  RELOC_NUMBER (R_ARM_RXPC25,         249)
#endif /* not OLD_ARM_ABI */
  RELOC_NUMBER (R_ARM_RSBREL32,       250)
  RELOC_NUMBER (R_ARM_THM_RPC22,      251)
  RELOC_NUMBER (R_ARM_RREL32,         252)
  RELOC_NUMBER (R_ARM_RABS32,         253)
  RELOC_NUMBER (R_ARM_RPC24,          254)
  RELOC_NUMBER (R_ARM_RBASE,          255)
END_RELOC_NUMBERS (R_ARM_max)

/* The name of the note section used to identify arm variants.  */
#define ARM_NOTE_SECTION ".note.gnu.arm.ident"
