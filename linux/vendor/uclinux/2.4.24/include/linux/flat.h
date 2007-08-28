/*
 * Copyright (C) 2002-2003  David McCullough <davidm@snapgear.com>
 * Copyright (C) 1998       Kenneth Albanowski <kjahds@kjahds.com>
 *                          The Silver Hammer Group, Ltd.
 *
 * This file provides the definitions and structures needed to
 * support uClinux flat-format executables.
 */

#ifndef _LINUX_FLAT_H
#define _LINUX_FLAT_H

#ifdef __KERNEL__
#include <asm/flat.h>
#endif

#define	FLAT_VERSION			0x00000004L

#ifdef CONFIG_BINFMT_SHARED_FLAT
#define	MAX_SHARED_LIBS			(4)
#else
#define	MAX_SHARED_LIBS			(1)
#endif

/*
 * To make everything easier to port and manage cross platform
 * development,  all fields are in network byte order.
 */

struct flat_hdr {
	char magic[4];
	unsigned long rev;          /* version (as above) */
	unsigned long entry;        /* Offset of first executable instruction
	                               with text segment from beginning of file */
	unsigned long data_start;   /* Offset of data segment from beginning of
	                               file */
	unsigned long data_end;     /* Offset of end of data segment
	                               from beginning of file */
	unsigned long bss_end;      /* Offset of end of bss segment from beginning
	                               of file */

	/* (It is assumed that data_end through bss_end forms the bss segment.) */

	unsigned long stack_size;   /* Size of stack, in bytes */
	unsigned long reloc_start;  /* Offset of relocation records from
	                               beginning of file */
	unsigned long reloc_count;  /* Number of relocation records */
	unsigned long flags;       
	unsigned long build_date;   /* When the program/library was built */
	unsigned long filler[5];    /* Reservered, set to zero */
};

#define FLAT_FLAG_RAM    0x0001 /* load program entirely into RAM */
#define FLAT_FLAG_GOTPIC 0x0002 /* program is PIC with GOT */
#define FLAT_FLAG_GZIP   0x0004 /* all but the header is compressed */
#define FLAT_FLAG_GZDATA 0x0008 /* only data/relocs are compressed (for XIP) */
#define FLAT_FLAG_KTRACE 0x0010 /* output useful kernel trace for debugging */


#ifdef __KERNEL__ /* so systems without linux headers can compile the apps */
/*
 * While it would be nice to keep this header clean,  users of older
 * tools still need this support in the kernel.  So this section is
 * purely for compatibility with old tool chains.
 *
 * DO NOT make changes or enhancements to the old format please,  just work
 *        with the format above,  except to fix bugs with old format support.
 */

#include <asm/byteorder.h>

#define	OLD_FLAT_VERSION			0x00000002L
#define OLD_FLAT_RELOC_TYPE_TEXT	0
#define OLD_FLAT_RELOC_TYPE_DATA	1
#define OLD_FLAT_RELOC_TYPE_BSS		2

typedef union {
	unsigned long	value;
	struct {
# if defined(mc68000) && !defined(CONFIG_COLDFIRE)
		signed long offset : 30;
		unsigned long type : 2;
#   	define OLD_FLAT_FLAG_RAM    0x1 /* load program entirely into RAM */
# elif defined(__BIG_ENDIAN_BITFIELD)
		unsigned long type : 2;
		signed long offset : 30;
#   	define OLD_FLAT_FLAG_RAM    0x1 /* load program entirely into RAM */
# elif defined(__LITTLE_ENDIAN_BITFIELD)
		signed long offset : 30;
		unsigned long type : 2;
#   	define OLD_FLAT_FLAG_RAM    0x1 /* load program entirely into RAM */
# else
#   	error "Unknown bitfield order for flat files."
# endif
	} reloc;
} flat_v2_reloc_t;

/* 
 * Version 5 relocation records.
 * =============================
 *
 * +--+-+---+--------------------------+
 * |  | |   |                          |
 * +--+-+---+--------------------------+
 * |  | |   |                          |  
 * |  | |   +--------------------------+-------------- offset
 * |  | |   |
 * |  | +---+----------------------------------------- sp
 * |  | |
 * |  +-+--------------------------------------------- hi_lo
 * |  |
 * +--+----------------------------------------------- type
 *
 * offset - This is the offset of where the relocation must take place.
 *          The offset is from the start of the section.  To find out 
 *          which section to look at, see type.
 * sp     - These bits are special platform specific bits.  See below for 
 *          further explanation and mapping for supported platforms.
 * hi_lo  - For relocations where only part of the address is taken, then
 *          this bit indicates whether it is the high part or the low part of
 *          the address.
 *          0 - Low part of address
 *          1 - High part of address
 * type   - This indicates which section the relocation is in.
 *          00 - TEXT section
 *          01 - DATA section
 *          10 - BSS section
 *
 * MIPS sp Specification
 * =====================
 *
 * +-+-+-+
 * | | | |
 * +-+-+-+
 * | | | |
 * | | +-+-------------------------------------------- carry
 * | | |
 * +-+-+---------------------------------------------- reloc_type
 *
 * carry      - This indicates whether the high part of the relcation address 
 *              requires a carry because of the low part calculations.
 *              0 - Do not carry from low part
 *              1 - Carry from low part
 * reloc_type - For MIPS, there are three types of relocations to handle.
 *              16-bit High part, 16-bit Low part, and 26-bit.  Thus this 
 *              indicates whether it is a hi/lo reloc or a 26-bit reloc.
 *              00 - 16-bit hi/lo relocation
 *              01 - 26-bit relocation
 *              10 - 32-bit relocation
 *
 * BLACKfin sp Specification
 * =========================
 * 
 * 000 luimm16 and huimm16. hi_lo used for selecting.
 * 001 pcrel24.
 * 010 abs32.
 *
 * Blackfin has too many kinds of relocations to fit into 3 bits. Only those
 * types of relocations that are required for compiled code are supported at
 * this time. Unlike mips tools, we don't have enough support for carry. The
 * kernel has to do ad-hoc carry calculations for huimm16 relocations. It
 * doesn't cause any problems for compiler generated code because a compiler
 * always generates a luimm16 type instruction followed by a huimm16
 * instruction. - akale
 *
 * SPARC sp Specification
 * ======================
 *
 * This architecture is not yet defined.
 *
 * ARM and M68K Details
 * ====================
 * These architectures use the version 4 bFLT format.  The version 4 format is 
 * a simple unsigned long integer (32-bits).  This record holds the relocation
 * offset from the start of text.  The loader must check for size of offset 
 * against the size of text section for XIP to work properly.
 *
 *
 * Faisal Akber
 *
 */
typedef union {
	unsigned long value;
	struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
		unsigned long type	:  2;
		unsigned long hi_lo	:  1;
		unsigned long sp	:  3;
		signed long   offset	: 26;
#elif defined(__BIG_ENDIAN_BITFIELD)
		signed long   offset	: 26;
		unsigned long sp	:  3;
		unsigned long hi_lo	:  1;
		unsigned long type	:  2;
#else
#  error "Unknown bitfield order for flat files."
#endif
	} reloc;
} flat_v5_reloc_t;

#define V5_FLAT_VERSION		0x00000005L
#define FLAT_V5_RELOC_TYPE_TEXT	0
#define FLAT_V5_RELOC_TYPE_DATA	1
#define FLAT_V5_RELOC_TYPE_BSS	2
#define FLAT_V5_RELOC_PART_LO	0
#define FLAT_V5_RELOC_PART_HI	1

#ifdef CONFIG_MIPS
#  define TARGET_MIPS
#  define FLAT_MIPS_RELOC_SP(type, carry) ((type * 2) + (carry))
#  define FLAT_MIPS_RELOC_SP_TYPE_16_BIT	0
#  define FLAT_MIPS_RELOC_SP_TYPE_24_BIT	1
#  define FLAT_MIPS_RELOC_SP_TYPE_32_BIT	2
#  define FLAT_MIPS_RELOC_SP_NO_CARRY	0
#  define FLAT_MIPS_RELOC_SP_CARRY		1
#endif

#ifdef CONFIG_BLACKFIN
#  define TARGET_BLACKFIN
#  define FLAT_BLACKFIN_RELOC_SP_TYPE_16_BIT	0
#  define FLAT_BLACKFIN_RELOC_SP_TYPE_24_BIT	1
#  define FLAT_BLACKFIN_RELOC_SP_TYPE_32_BIT	2
#endif

#endif /* __KERNEL__ */

#endif /* _LINUX_FLAT_H */
