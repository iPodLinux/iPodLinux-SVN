/* 
 * inftlcore.c -- Linux driver for Inverse Flash Translation Layer (INFTL)
 *
 * (C) Copyright 2002, Greg Ungerer (gerg@snapgear.com)
 *
 * Based heavily on the inftlcore.c code which is:
 * (c) 1999 Machine Vision Holdings, Inc.
 * Author: David Woodhouse <dwmw2@infradead.org>
 *
 * $Id: inftlcore.c,v 1.2 2004/01/12 12:22:27 davidm Exp $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/kmod.h>
#include <linux/hdreg.h>
#include <linux/blkpg.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nftl.h>
#include <linux/mtd/inftl.h>
#include <asm/uaccess.h>
#include <asm/errno.h>
#include <asm/io.h>

/*
 * Maximum number of loops while examining next block, to have a
 * chance to detect consistency problems (they should never happen
 * because of the checks done in the mounting.
 */
#define MAX_LOOPS 10000

/* INFTL block device stuff */
#define MAJOR_NR INFTL_MAJOR
#define DEVICE_OFF(device)

#include <linux/blk.h>
#include <linux/hdreg.h>

/*
 * There is some confusion as to what major inftl actually can use.
 */
#undef DEVICE_REQUEST
#undef DEVICE_NR
#define DEVICE_REQUEST inftl_request
#define	DEVICE_NR(device) (MINOR(device) >> PARTN_BITS)


extern void INFTL_dumptables(struct INFTLrecord *inftl);
extern void INFTL_dumpVUchains(struct INFTLrecord *inftl);


/* Linux-specific block device functions */

/* I _HATE_ the Linux block device setup more than anything else I've ever
 *  encountered, except ...
 */

static int inftl_sizes[256];
static int inftl_blocksizes[256];

/* .. for the Linux partition table handling. */
static struct hd_struct part_table[256];

static struct gendisk inftl_gendisk = {
	.major =	MAJOR_NR,
	.major_name =	"inftl",
	.minor_shift =	INFTL_PARTN_BITS, /* Bits to shift to get real from partition */
	.max_p =	(1<<INFTL_PARTN_BITS)-1, /* Number of partitions per real */
	.part =		part_table,     /* hd struct */
	.sizes =	inftl_sizes,     /* block sizes */
};

struct INFTLrecord *INFTLs[MAX_INFTLS];

static void INFTL_setup(struct mtd_info *mtd)
{
	int i;
	struct INFTLrecord *inftl;
	unsigned long temp;
	int firstfree = -1;

	DEBUG(MTD_DEBUG_LEVEL1, "INFTL: INFTL_setup()\n");

	for (i = 0; i < MAX_INFTLS; i++) {
		if (!INFTLs[i] && firstfree == -1)
			firstfree = i;
		else if (INFTLs[i] && INFTLs[i]->mtd == mtd) {
			/*
			 * This is a Spare Media Header for an INFTL we've
			 * already found.
			 */
			DEBUG(MTD_DEBUG_LEVEL1, "INFTL: MTD already mounted "
				"as INFTL\n");
			return;
		}
	}
        if (firstfree == -1) {
		printk(KERN_WARNING "INFTL: No more INFTL slots available\n");
		return;
        }

	inftl = kmalloc(sizeof(struct INFTLrecord), GFP_KERNEL);
	if (!inftl) {
		printk(KERN_WARNING "INFTL: out of memory for INFTL data "
			"structures\n");
		return;
	}

	init_MUTEX(&inftl->mutex);
	inftl->mtd = mtd;

        if (INFTL_mount(inftl) < 0) {
		printk(KERN_WARNING "INFTL:cCould not mount INFTL device\n");
		kfree(inftl);
		return;
        }

	/* OK, it's a new one. Set up all the data structures. */
#ifdef PSYCHO_DEBUG
	printk("INFTL: found new INFTL inftl%c\n", firstfree + 'a');
#endif

        /* linux stuff */
	inftl->usecount = 0;
	inftl->cylinders = 1024;
	inftl->heads = 16;

	temp = inftl->cylinders * inftl->heads;
	inftl->sectors = inftl->nr_sects / temp;
	if (inftl->nr_sects % temp) {
		inftl->sectors++;
		temp = inftl->cylinders * inftl->sectors;
		inftl->heads = inftl->nr_sects / temp;

		if (inftl->nr_sects % temp) {
			inftl->heads++;
			temp = inftl->heads * inftl->sectors;
			inftl->cylinders = inftl->nr_sects / temp;
		}
	}

	if (inftl->nr_sects != inftl->heads*inftl->cylinders*inftl->sectors) {
		printk(KERN_WARNING "INFTL: cannot calculate an INFTL "
			"geometry to match size of 0x%x.\n",
			(int)inftl->nr_sects);
		printk(KERN_WARNING "INFTL: Using C:%d H:%d S:%d (== 0x%lx "
			"sects)\n", inftl->cylinders, inftl->heads,
			inftl->sectors, (long)inftl->cylinders *
			(long)inftl->heads * (long)inftl->sectors );

		/*
		 * Oh no we don't have inftl->nr_sects = inftl->heads *
		 * inftl->cylinders * inftl->sectors;
		 */
	}
	INFTLs[firstfree] = inftl;
	/* Finally, set up the block device sizes */
	inftl_sizes[firstfree * 16] = inftl->nr_sects;
	//nftl_blocksizes[firstfree*16] = 512;
	part_table[firstfree * 16].nr_sects = inftl->nr_sects;

	inftl_gendisk.nr_real++;

	/* partition check ... */
	grok_partitions(&inftl_gendisk, firstfree, 1 << INFTL_PARTN_BITS,
		inftl->nr_sects);
}

static void INFTL_unsetup(int i)
{
	struct INFTLrecord *inftl = INFTLs[i];

	DEBUG(MTD_DEBUG_LEVEL1, "INFTL: INFTL_unsetup(%d)\n", i);
	
	INFTLs[i] = NULL;
	
	if (inftl->PUtable)
		kfree(inftl->PUtable);
	if (inftl->VUtable)
		kfree(inftl->VUtable);
		      
	inftl_gendisk.nr_real--;
	kfree(inftl);
}

/*
 * Search the MTD device for INFTL partitions
 */
static void INFTL_notify_add(struct mtd_info *mtd)
{
	DEBUG(MTD_DEBUG_LEVEL1, "INFTL: INFTL_notify_add(%s)\n", mtd->name);

	if (mtd) {
		if (!mtd->read_oob) {
			/*
			 * If this MTD doesn't have out-of-band data,
			 * then there's no point continuing.
			 */
			DEBUG(MTD_DEBUG_LEVEL1, "INFTL: no OOB data, quitting\n");
			return;
		}
		DEBUG(MTD_DEBUG_LEVEL3, "INFTL: mtd->read = %p, size = %d, "
			"erasesize = %d\n", mtd->read, mtd->size,
			mtd->erasesize);

                INFTL_setup(mtd);
	}
}

static void INFTL_notify_remove(struct mtd_info *mtd)
{
	int i;

	for (i = 0; i < MAX_INFTLS; i++) {
		if (INFTLs[i] && INFTLs[i]->mtd == mtd)
			INFTL_unsetup(i);
	}
}

/*
 * Actual INFTL access routines.
 */

/*
 * INFTL_findfreeblock: Find a free Erase Unit on the INFTL partition.
 *	This function is used when the give Virtual Unit Chain.
 */
static u16 INFTL_findfreeblock(struct INFTLrecord *inftl, int desperate)
{
	u16 pot = inftl->LastFreeEUN;
	int silly = inftl->nb_blocks;

	DEBUG(MTD_DEBUG_LEVEL3, "INFTL: INFTL_findfreeblock(inftl=0x%x,"
		"desperate=%d)\n", (int)inftl, desperate);

	/*
	 * Normally, we force a fold to happen before we run out of free
	 * blocks completely.
	 */
	if (!desperate && inftl->numfreeEUNs < 2) {
		DEBUG(MTD_DEBUG_LEVEL1, "INFTL: there are too few free "
			"EUNs (%d)\n", inftl->numfreeEUNs);
		return 0xffff;
	}

	/* Scan for a free block */
	do {
		if (inftl->PUtable[pot] == BLOCK_FREE) {
			inftl->LastFreeEUN = pot;
			return pot;
		}

		if (++pot > inftl->lastEUN)
			pot = 0;

		if (!silly--) {
			printk(KERN_WARNING "INFTL: no free blocks found!  "
				"EUN range = %d - %d\n", 0, inftl->LastFreeEUN);
			return BLOCK_NIL;
		}
	} while (pot != inftl->LastFreeEUN);

	return BLOCK_NIL;
}

static u16 INFTL_foldchain(struct INFTLrecord *inftl, unsigned thisVUC, unsigned pendingblock)
{
	u16 BlockMap[MAX_SECTORS_PER_UNIT];
	unsigned char BlockDeleted[MAX_SECTORS_PER_UNIT];
	unsigned int thisEUN, prevEUN, status;
	int block, silly;
	unsigned int targetEUN;
	struct inftl_oob oob;
        size_t retlen;

	DEBUG(MTD_DEBUG_LEVEL3, "INFTL: INFTL_foldchain(inftl=0x%x,thisVUC=%d,"
		"pending=%d)\n", (int)inftl, thisVUC, pendingblock);

	memset(BlockMap, 0xff, sizeof(BlockMap));
	memset(BlockDeleted, 0, sizeof(BlockDeleted));

	thisEUN = targetEUN = inftl->VUtable[thisVUC];

	if (thisEUN == BLOCK_NIL) {
		printk(KERN_WARNING "INFTL: trying to fold non-existent "
		       "Virtual Unit Chain %d!\n", thisVUC);
		return BLOCK_NIL;
	}
	
	/*
	 * Scan to find the Erase Unit which holds the actual data for each
	 * 512-byte block within the Chain.
	 */
        silly = MAX_LOOPS;
	while (thisEUN < inftl->nb_blocks) {
		for (block = 0; block < inftl->EraseSize/SECTORSIZE; block ++) {
			if ((BlockMap[block] != 0xffff) || BlockDeleted[block])
				continue;

			if (MTD_READOOB(inftl->mtd, (thisEUN * inftl->EraseSize)
			     + (block * SECTORSIZE), 16 , &retlen,
			     (char *)&oob) < 0)
				status = SECTOR_IGNORE;
			else
                        	status = oob.b.Status | oob.b.Status1;

			switch(status) {
			case SECTOR_FREE:
			case SECTOR_IGNORE:
				break;
			case SECTOR_USED:
				BlockMap[block] = thisEUN;
				continue;
			case SECTOR_DELETED:
				BlockDeleted[block] = 1;
				continue;
			default:
				printk(KERN_WARNING "INFTL: unknown status "
					"for block %d in EUN %d: %x\n",
					block, thisEUN, status);
				break;
			}
		}

		if (!silly--) {
			printk(KERN_WARNING "INFTL: infinite loop in Virtual "
				"Unit Chain 0x%x\n", thisVUC);
			return BLOCK_NIL;
		}
		
		thisEUN = inftl->PUtable[thisEUN];
	}

	/*
	 * OK. We now know the location of every block in the Virtual Unit
	 * Chain, and the Erase Unit into which we are supposed to be copying.
	 * Go for it.
	 */
	DEBUG(MTD_DEBUG_LEVEL1, "INFTL: folding chain %d into unit %d\n",
		thisVUC, targetEUN);

	for (block = 0; block < inftl->EraseSize/SECTORSIZE ; block++) {
		unsigned char movebuf[SECTORSIZE];
		int ret;

		/*
		 * If it's in the target EUN already, or if it's pending write,
		 * do nothing.
		 */
		if (BlockMap[block] == targetEUN || (pendingblock ==
		    (thisVUC * (inftl->EraseSize / SECTORSIZE) + block))) {
			continue;
		}

                /*
		 * Copy only in non free block (free blocks can only
                 * happen in case of media errors or deleted blocks).
		 */
                if (BlockMap[block] == BLOCK_NIL)
                        continue;
                
                ret = MTD_READECC(inftl->mtd, (inftl->EraseSize *
			BlockMap[block]) + (block * SECTORSIZE), SECTORSIZE,
			&retlen, movebuf, (char *)&oob, NAND_ECC_DISKONCHIP); 
                if (ret < 0) {
			ret = MTD_READECC(inftl->mtd, (inftl->EraseSize *
				BlockMap[block]) + (block * SECTORSIZE),
				SECTORSIZE, &retlen, movebuf, (char *)&oob,
				NAND_ECC_DISKONCHIP); 
			if (ret != -EIO) 
                        	DEBUG(MTD_DEBUG_LEVEL1, "INFTL: error went "
					"away on retry?\n");
                }
                MTD_WRITEECC(inftl->mtd, (inftl->EraseSize * targetEUN) +
			(block * SECTORSIZE), SECTORSIZE, &retlen,
			movebuf, (char *)&oob, NAND_ECC_DISKONCHIP);
	}

	/*
	 * Newest unit in chain now contains data from _all_ older units.
	 * So go through and erase each unit in chain, oldest first. (This
	 * is important, by doing oldest first if we crash/reboot then it
	 * it is relatively simple to clean up the mess).
	 */
	DEBUG(MTD_DEBUG_LEVEL1, "INFTL: want to erase virtual chain %d\n",
		thisVUC);

	for (;;) {
		/* Find oldest unit in chain. */
		thisEUN = inftl->VUtable[thisVUC];
		prevEUN = BLOCK_NIL;
		while (inftl->PUtable[thisEUN] != BLOCK_NIL) {
			prevEUN = thisEUN;
			thisEUN = inftl->PUtable[thisEUN];
		}

		/* Check if we are all done */
		if (thisEUN == targetEUN)
			break;

                if (INFTL_formatblock(inftl, thisEUN) < 0) {
			/*
			 * Could not erase : mark block as reserved.
			 * FixMe: Update Bad Unit Table on disk.
			 */
			inftl->PUtable[thisEUN] = BLOCK_RESERVED;
                } else {
			/* Correctly erased : mark it as free */
			inftl->PUtable[thisEUN] = BLOCK_FREE;
			inftl->PUtable[prevEUN] = BLOCK_NIL;
			inftl->numfreeEUNs++;
                }
	}

	return targetEUN;
}

u16 INFTL_makefreeblock(struct INFTLrecord *inftl, unsigned pendingblock)
{
	/*
	 * This is the part that needs some cleverness applied. 
	 * For now, I'm doing the minimum applicable to actually
	 * get the thing to work.
	 * Wear-levelling and other clever stuff needs to be implemented
	 * and we also need to do some assessment of the results when
	 * the system loses power half-way through the routine.
	 */
	u16 LongestChain = 0;
	u16 ChainLength = 0, thislen;
	u16 chain, EUN;

	DEBUG(MTD_DEBUG_LEVEL3, "INFTL: INFTL_makefreeblock(inftl=0x%x,"
		"pending=%d)\n", (int)inftl, pendingblock);

	for (chain = 0; chain < inftl->nb_blocks; chain++) {
		EUN = inftl->VUtable[chain];
		thislen = 0;

		while (EUN <= inftl->lastEUN) {
			thislen++;
			EUN = inftl->PUtable[EUN];
			if (thislen > 0xff00) {
				printk(KERN_WARNING "INFTL: endless loop in "
					"Virtual Chain %d: Unit %x\n",
					chain, EUN);
				/*
				 * Actually, don't return failure.
				 * Just ignore this chain and get on with it.
				 */
				thislen = 0;
				break;
			}
		}

		if (thislen > ChainLength) {
			ChainLength = thislen;
			LongestChain = chain;
		}
	}

	if (ChainLength < 2) {
		printk(KERN_WARNING "INFTL: no Virtual Unit Chains available "
			"for folding. Failing request\n");
		return BLOCK_NIL;
	}

	return INFTL_foldchain(inftl, LongestChain, pendingblock);
}

static int nrbits(unsigned int val, int bitcount)
{
	int i, total = 0;

	for (i = 0; (i < bitcount); i++)
		total += (((0x1 << i) & val) ? 1 : 0);
	return total;
}

/*
 * INFTL_findwriteunit: Return the unit number into which we can write 
 *                      for this block. Make it available if it isn't already.
 */
static /*inline*/ u16 INFTL_findwriteunit(struct INFTLrecord *inftl, unsigned int block)
{
	unsigned int thisVUC = block / (inftl->EraseSize / SECTORSIZE);
	unsigned int thisEUN, writeEUN, prev_block, status;
	unsigned long blockofs = (block * SECTORSIZE) & (inftl->EraseSize -1);
	struct inftl_oob oob;
	struct inftl_bci bci;
	size_t retlen;
	int silly, silly2 = 3;
	unsigned char anac, nacs, parity;

	DEBUG(MTD_DEBUG_LEVEL3, "INFTL: INFTL_findwriteunit(inftl=0x%x,"
		"block=%d)\n", (int)inftl, block);

	do {
		/*
		 * Scan the media to find a unit in the VUC which has
		 * a free space for the block in question.
		 */
		writeEUN = BLOCK_NIL;
		thisEUN = inftl->VUtable[thisVUC];
		silly = MAX_LOOPS;

		while (thisEUN <= inftl->lastEUN) {
			MTD_READOOB(inftl->mtd, (thisEUN * inftl->EraseSize) +
				blockofs, 8, &retlen, (char *)&bci);

                        status = bci.Status | bci.Status1;
			DEBUG(MTD_DEBUG_LEVEL3, "INFTL: status of block %d in "
				"EUN %d is %x\n", block , writeEUN, status);

			switch(status) {
			case SECTOR_FREE:
				writeEUN = thisEUN;
				break;
			case SECTOR_DELETED:
			case SECTOR_USED:
				/* Can't go any further */
				goto hitused;
			case SECTOR_IGNORE:
				break;
			default:
				/*
				 * Invalid block. Don't use it any more.
				 * Must implement.
				 */
				break;			
			}
			
			if (!silly--) { 
				printk(KERN_WARNING "INFTL: infinite loop in "
					"Virtual Unit Chain 0x%x\n", thisVUC);
				return 0xffff;
			}

			/* Skip to next block in chain */
			thisEUN = inftl->PUtable[thisEUN];
		}

hitused:
		if (writeEUN != BLOCK_NIL)
			return writeEUN;


		/*
		 * OK. We didn't find one in the existing chain, or there 
		 * is no existing chain. Allocate a new one.
		 */
		writeEUN = INFTL_findfreeblock(inftl, 0);

		if (writeEUN == BLOCK_NIL) {
			/*
			 * That didn't work - there were no free blocks just
			 * waiting to be picked up. We're going to have to fold
			 * a chain to make room.
			 */
			thisEUN = INFTL_makefreeblock(inftl, 0xffff);

			/*
			 * Hopefully we free something, lets try again.
			 * This time we are desperate...
			 */
			DEBUG(MTD_DEBUG_LEVEL1, "INFTL: using desperate==1 "
				"to find free EUN to accommodate write to "
				"VUC %d\n", thisVUC);
			writeEUN = INFTL_findfreeblock(inftl, 1);
			if (writeEUN == BLOCK_NIL) {
				/*
				 * Ouch. This should never happen - we should
				 * always be able to make some room somehow. 
				 * If we get here, we've allocated more storage 
				 * space than actual media, or our makefreeblock
				 * routine is missing something.
				 */
				printk(KERN_WARNING "INFTL: cannot make free "
					"space.\n");
#ifdef DEBUG
				INFTL_dumptables(inftl);
				INFTL_dumpVUchains(inftl);
#endif
				return BLOCK_NIL;
			}			
		}

		/*
		 * Insert new block into virtual chain. Firstly update the
		 * block headers in flash...
		 */
		anac = 0;
		nacs = 0;
		thisEUN = inftl->VUtable[thisVUC];

		if (thisEUN != BLOCK_NIL) {
			MTD_READOOB(inftl->mtd, thisEUN * inftl->EraseSize
				+ 8, 8, &retlen, (char *)&oob.u);
			anac = oob.u.a.ANAC + 1;
			nacs = oob.u.a.NACs + 1;
		}

		prev_block = inftl->VUtable[thisVUC];
		if (prev_block < inftl->nb_blocks)
			prev_block -= inftl->firstEUN;

		parity = (nrbits(thisVUC, 16) & 0x1) ? 0x1 : 0;
		parity |= (nrbits(prev_block, 16) & 0x1) ? 0x2 : 0;
		parity |= (nrbits(anac, 8) & 0x1) ? 0x4 : 0;
		parity |= (nrbits(nacs, 8) & 0x1) ? 0x8 : 0;
 
		oob.u.a.virtualUnitNo = cpu_to_le16(thisVUC);
		oob.u.a.prevUnitNo = cpu_to_le16(prev_block);
		oob.u.a.ANAC = anac;
		oob.u.a.NACs = nacs;
		oob.u.a.parityPerField = parity;
		oob.u.a.discarded = 0xaa;

		MTD_WRITEOOB(inftl->mtd, writeEUN * inftl->EraseSize + 8, 8,
			&retlen, (char *)&oob.u);

		/* Also back up header... */
		oob.u.b.virtualUnitNo = cpu_to_le16(thisVUC);
		oob.u.b.prevUnitNo = cpu_to_le16(prev_block);
		oob.u.b.ANAC = anac;
		oob.u.b.NACs = nacs;
		oob.u.b.parityPerField = parity;
		oob.u.b.discarded = 0xaa;

		MTD_WRITEOOB(inftl->mtd, writeEUN * inftl->EraseSize + 
			SECTORSIZE * 4 + 8, 8, &retlen, (char *)&oob.u);

		inftl->PUtable[writeEUN] = inftl->VUtable[thisVUC];
		inftl->VUtable[thisVUC] = writeEUN;

		inftl->numfreeEUNs--;
		return writeEUN;

	} while (silly2--);

	printk(KERN_WARNING "INFTL: error folding to make room for Virtual "
		"Unit Chain 0x%x\n", thisVUC);
	return 0xffff;
}

/*
 * Given a Virtual Unit Chain, see if it can be deleted, and if so do it.
 */
static void INFTL_trydeletechain(struct INFTLrecord *inftl, unsigned thisVUC)
{
	unsigned char BlockUsed[MAX_SECTORS_PER_UNIT];
	unsigned char BlockDeleted[MAX_SECTORS_PER_UNIT];
	unsigned int thisEUN, status;
	int block, silly;
	struct inftl_bci bci;
	size_t retlen;

	DEBUG(MTD_DEBUG_LEVEL3, "INFTL: INFTL_trydeletechain(inftl=0x%x,"
		"thisVUC=%d)\n", (int)inftl, thisVUC);

	memset(BlockUsed, 0, sizeof(BlockUsed));
	memset(BlockDeleted, 0, sizeof(BlockDeleted));

	thisEUN = inftl->VUtable[thisVUC];
	if (thisEUN == BLOCK_NIL) {
		printk(KERN_WARNING "INFTL: trying to delete non-existent "
		       "Virtual Unit Chain %d!\n", thisVUC);
		return;
	}
	
	/*
	 * Scan through the Erase Units to determine whether any data is in
	 * each of the 512-byte blocks within the Chain.
	 */
	silly = MAX_LOOPS;
	while (thisEUN < inftl->nb_blocks) {
		for (block = 0; block < inftl->EraseSize/SECTORSIZE; block++) {
			if (BlockUsed[block] || BlockDeleted[block])
				continue;

			if (MTD_READOOB(inftl->mtd, (thisEUN * inftl->EraseSize)
			    + (block * SECTORSIZE), 8 , &retlen,
			    (char *)&bci) < 0)
				status = SECTOR_IGNORE;
			else
				status = bci.Status | bci.Status1;

			switch(status) {
			case SECTOR_FREE:
			case SECTOR_IGNORE:
				break;
			case SECTOR_USED:
				BlockUsed[block] = 1;
				continue;
			case SECTOR_DELETED:
				BlockDeleted[block] = 1;
				continue;
			default:
				printk(KERN_WARNING "INFTL: unknown status "
					"for block %d in EUN %d: 0x%x\n",
					block, thisEUN, status);
			}
		}

		if (!silly--) {
			printk(KERN_WARNING "INFTL: infinite loop in Virtual "
				"Unit Chain 0x%x\n", thisVUC);
			return;
		}
		
		thisEUN = inftl->PUtable[thisEUN];
	}

	for (block = 0; block < inftl->EraseSize/SECTORSIZE; block++)
		if (BlockUsed[block])
			return;

	/*
	 * For each block in the chain free it and make it available
	 * for future use. Erase from the oldest unit first.
	 */
	DEBUG(MTD_DEBUG_LEVEL1, "INFTL: deleting empty VUC %d\n", thisVUC);

	for (;;) {
		u16 *prevEUN = &inftl->VUtable[thisVUC];
		thisEUN = *prevEUN;

		/* If the chain is all gone already, we're done */
		if (thisEUN == BLOCK_NIL) {
			DEBUG(MTD_DEBUG_LEVEL2, "INFTL: Empty VUC %d for deletion was already absent\n", thisEUN);
			return;
		}

		/* Find oldest unit in chain. */
		while (inftl->PUtable[thisEUN] != BLOCK_NIL) {
			BUG_ON(thisEUN >= inftl->nb_blocks);

			prevEUN = &inftl->PUtable[thisEUN];
			thisEUN = *prevEUN;
		}

		DEBUG(MTD_DEBUG_LEVEL3, "Deleting EUN %d from VUC %d\n",
		      thisEUN, thisVUC);

                if (INFTL_formatblock(inftl, thisEUN) < 0) {
			/*
			 * Could not erase : mark block as reserved.
			 * FixMe: Update Bad Unit Table on medium.
			 */
			inftl->PUtable[thisEUN] = BLOCK_RESERVED;
                } else {
			/* Correctly erased : mark it as free */
			inftl->PUtable[thisEUN] = BLOCK_FREE;
			inftl->numfreeEUNs++;
		}

		/* Now sort out whatever was pointing to it... */
		*prevEUN = BLOCK_NIL;

		/* Ideally we'd actually be responsive to new
		   requests while we're doing this -- if there's
		   free space why should others be made to wait? */
		cond_resched();
	}

	inftl->VUtable[thisVUC] = BLOCK_NIL;
}

static int INFTL_deleteblock(struct INFTLrecord *inftl, unsigned block)
{
	unsigned int thisEUN = inftl->VUtable[block / (inftl->EraseSize / SECTORSIZE)];
	unsigned long blockofs = (block * SECTORSIZE) & (inftl->EraseSize - 1);
	unsigned int status;
	int silly = MAX_LOOPS;
	size_t retlen;
	struct inftl_bci bci;

	DEBUG(MTD_DEBUG_LEVEL3, "INFTL: INFTL_deleteblock(inftl=0x%x,"
		"block=%d)\n", (int)inftl, block);

	while (thisEUN < inftl->nb_blocks) {
		if (MTD_READOOB(inftl->mtd, (thisEUN * inftl->EraseSize) +
		    blockofs, 8, &retlen, (char *)&bci) < 0)
			status = SECTOR_IGNORE;
		else
			status = bci.Status | bci.Status1;

		switch (status) {
		case SECTOR_FREE:
		case SECTOR_IGNORE:
			break;
		case SECTOR_DELETED:
			thisEUN = BLOCK_NIL;
			goto foundit;
		case SECTOR_USED:
			goto foundit;
		default:
			printk(KERN_WARNING "INFTL: unknown status for "
				"block %d in EUN %d: 0x%x\n",
				block, thisEUN, status);
			break;
		}

		if (!silly--) {
			printk(KERN_WARNING "INFTL: infinite loop in Virtual "
				"Unit Chain 0x%x\n",
				block / (inftl->EraseSize / SECTORSIZE));
			return 1;
		}
		thisEUN = inftl->PUtable[thisEUN];
	}

foundit:
	if (thisEUN != BLOCK_NIL) {
		loff_t ptr = (thisEUN * inftl->EraseSize) + blockofs;

		if (MTD_READOOB(inftl->mtd, ptr, 8, &retlen, (char *)&bci) < 0)
			return -EIO;
		bci.Status = bci.Status1 = SECTOR_DELETED;
		if (MTD_WRITEOOB(inftl->mtd, ptr, 8, &retlen, (char *)&bci) < 0)
			return -EIO;
		INFTL_trydeletechain(inftl, block / (inftl->EraseSize / SECTORSIZE));
	}
	return 0;
}

static int INFTL_writeblock(struct INFTLrecord *inftl, unsigned block, char *buffer)
{
	u16 writeEUN;
	unsigned long blockofs = (block * SECTORSIZE) & (inftl->EraseSize - 1);
	size_t retlen;
	u8 eccbuf[6];
	char *p, *pend;

	DEBUG(MTD_DEBUG_LEVEL3, "INFTL: inftl_writeblock(inftl=0x%x,block=%d,"
		"buffer=0x%x)\n", (int)inftl, block, (int)buffer);

	/* Is block all zero? */
	pend = buffer + SECTORSIZE;
	for (p = buffer; p < pend && !*p; p++);

	if (p < pend) {
		writeEUN = INFTL_findwriteunit(inftl, block);

		if (writeEUN == BLOCK_NIL) {
			printk(KERN_WARNING "INFTL__writeblock(): cannot find "
				"block to write to\n");
			/*
			 * If we _still_ haven't got a block to use,
			 * we're screwed.
			 */
			return 1;
		}

		MTD_WRITEECC(inftl->mtd, (writeEUN * inftl->EraseSize) +
			blockofs, SECTORSIZE, &retlen, (char *)buffer,
			(char *)eccbuf, NAND_ECC_DISKONCHIP);
		/*
		 * no need to write SECTOR_USED flags since they are
		 * written in mtd_writeecc.
		 */
	} else {
		INFTL_deleteblock(inftl, block);
	}

	return 0;
}

static int INFTL_readblock(struct INFTLrecord *inftl, unsigned block, char *buffer)
{
	u16 thisEUN = inftl->VUtable[block / (inftl->EraseSize / SECTORSIZE)];
	unsigned long blockofs = (block * SECTORSIZE) & (inftl->EraseSize - 1);
        unsigned int status;
	int silly = MAX_LOOPS;
        struct inftl_bci bci;
        size_t retlen;

	DEBUG(MTD_DEBUG_LEVEL3, "INFTL: inftl_readblock(inftl=0x%x,block=%d,"
		"buffer=0x%x)\n", (int)inftl, block, (int)buffer);

	while (thisEUN < inftl->nb_blocks) {
		if (MTD_READOOB(inftl->mtd, (thisEUN * inftl->EraseSize) +
		     blockofs, 8, &retlen, (char *)&bci) < 0)
			status = SECTOR_IGNORE;
		else
			status = bci.Status | bci.Status1;

		switch (status) {
		case SECTOR_DELETED:
			thisEUN = BLOCK_NIL;
			goto foundit;
		case SECTOR_USED:
			goto foundit;
		case SECTOR_FREE:
		case SECTOR_IGNORE:
			break;
		default:
			printk(KERN_WARNING "INFTL: unknown status for "
				"block %d in EUN %d: 0x%04x\n",
				block, thisEUN, status);
			break;
		}

		if (!silly--) {
			printk(KERN_WARNING "INFTL: infinite loop in "
				"Virtual Unit Chain 0x%x\n",
				block / (inftl->EraseSize / SECTORSIZE));
			return 1;
		}

		thisEUN = inftl->PUtable[thisEUN];
	}

foundit:
	if (thisEUN == BLOCK_NIL) {
		/* The requested block is not on the media, return all 0x00 */
		memset(buffer, 0, SECTORSIZE);
	} else {
        	size_t retlen;
		loff_t ptr = (thisEUN * inftl->EraseSize) + blockofs;
		u_char eccbuf[6];
		if (MTD_READECC(inftl->mtd, ptr, SECTORSIZE, &retlen,
		    buffer, eccbuf, NAND_ECC_DISKONCHIP))
			return -EIO;
	}
	return 0;
}

static int inftl_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct INFTLrecord *inftl;
	int p;

	inftl = INFTLs[MINOR(inode->i_rdev) >> INFTL_PARTN_BITS];

	if (!inftl)
		return -EINVAL;

	switch (cmd) {
#if 0
	case INFTL_IOCTL_DELETE_SECTOR: {
		unsigned int dev;
		int res;

		if (arg >= part_table[dev].nr_sects)
			return -EINVAL;
		arg += part_table[MINOR(inode->i_rdev)].start_sect;
		DEBUG(MTD_DEBUG_LEVEL3, "Waiting for mutex\n");
		down(&inftl->mutex);
		DEBUG(MTD_DEBUG_LEVEL3, "Got mutex\n");
		res = INFTL_deleteblock(inftl, arg);
		up(&inftl->mutex);
		return res;
	}
#endif
	case HDIO_GETGEO: {
		struct hd_geometry g;

		g.heads = inftl->heads;
		g.sectors = inftl->sectors;
		g.cylinders = inftl->cylinders;
		g.start = part_table[MINOR(inode->i_rdev)].start_sect;
		return copy_to_user((void *)arg, &g, sizeof g) ? -EFAULT : 0;
	}
	case BLKGETSIZE:   /* Return device size */
		return put_user(part_table[MINOR(inode->i_rdev)].nr_sects,
			(unsigned long *) arg);

#ifdef BLKGETSIZE64
	case BLKGETSIZE64:
		return put_user((u64)part_table[MINOR(inode->i_rdev)].nr_sects << 9,
			(u64 *)arg);
#endif

	case BLKFLSBUF:
		if (!capable(CAP_SYS_ADMIN))
			return -EACCES;
		fsync_dev(inode->i_rdev);
		invalidate_buffers(inode->i_rdev);
		if (inftl->mtd->sync)
			inftl->mtd->sync(inftl->mtd);
		return 0;

	case BLKRRPART:
		if (!capable(CAP_SYS_ADMIN))
			return -EACCES;
		if (inftl->usecount > 1)
			return -EBUSY;
		/* 
		 * We have to flush all buffers and invalidate caches,
		 * or we won't be able to re-use the partitions,
		 * if there was a change and we don't want to reboot
		 */
		p = (1 << INFTL_PARTN_BITS) - 1;
		while (p-- > 0) {
			kdev_t devp = MKDEV(MAJOR(inode->i_dev),
				MINOR(inode->i_dev)+p);
			if (part_table[p].nr_sects > 0)
				invalidate_device (devp, 1);

			part_table[MINOR(inode->i_dev)+p].start_sect = 0;
			part_table[MINOR(inode->i_dev)+p].nr_sects = 0;
		}
		
#if LINUX_VERSION_CODE < 0x20328
		resetup_one_dev(&inftl_gendisk,
			MINOR(inode->i_rdev) >> INFTL_PARTN_BITS);
#else
		grok_partitions(&inftl_gendisk,
			MINOR(inode->i_rdev) >> INFTL_PARTN_BITS,
			1 << INFTL_PARTN_BITS, inftl->nr_sects);
#endif
		return 0;

	case BLKROSET:
	case BLKROGET:
	case BLKSSZGET:
		return blk_ioctl(inode->i_rdev, cmd, arg);

	default:
		break;
	}

	return -EINVAL;
}

void inftl_request(RQFUNC_ARG)
{
	unsigned int dev, block, nsect;
	struct INFTLrecord *inftl;
	char *buffer;
	struct request *req;
	int res;

	while (1) {
		INIT_REQUEST;	/* blk.h */
		req = CURRENT;
		
		/* We can do this because the generic code knows not to
		   touch the request at the head of the queue */
		spin_unlock_irq(&io_request_lock);

		DEBUG(MTD_DEBUG_LEVEL2, "INFTL_request\n");
		DEBUG(MTD_DEBUG_LEVEL3, "INFTL %s request, from sector 0x%04lx"
			" for 0x%04lx sectors\n",
			(req->cmd == READ) ? "Read " : "Write",
			req->sector, req->current_nr_sectors);

		dev = MINOR(req->rq_dev);
		block = req->sector;
		nsect = req->current_nr_sectors;
		buffer = req->buffer;
		res = 1; /* succeed */

		if (dev >= MAX_INFTLS * (1 << INFTL_PARTN_BITS)) {
			/* there is no such partition */
			printk("INFTL: bad minor number: device = %s\n",
			       kdevname(req->rq_dev));
			res = 0; /* fail */
			goto repeat;
		}
		
		inftl = INFTLs[dev / (1 << INFTL_PARTN_BITS)];
		DEBUG(MTD_DEBUG_LEVEL3, "Waiting for mutex\n");
		down(&inftl->mutex);
		DEBUG(MTD_DEBUG_LEVEL3, "Got mutex\n");

		if (block + nsect > part_table[dev].nr_sects) {
			/* access past the end of device */
			printk("inftl%c%d: bad access: block = %d, "
				"count = %d\n",
				(MINOR(req->rq_dev) >> 6) + 'a', dev & 0xf,
				block, nsect);
			up(&inftl->mutex);
			res = 0; /* fail */
			goto repeat;
		}
		
		block += part_table[dev].start_sect;
		
		if (req->cmd == READ) {
			DEBUG(MTD_DEBUG_LEVEL2, "INFTL: read request of 0x%x"
				"  sectors @ %x (req->nr_sectors == %lx)\n",
				nsect, block, req->nr_sectors);
	
			for ( ; nsect > 0; nsect-- , block++, buffer += SECTORSIZE) {
				/* Read a single sector to req->buffer + (512 * i) */
				if (INFTL_readblock(inftl, block, buffer)) {
					DEBUG(MTD_DEBUG_LEVEL2, "INFTL: "
						"read request failed\n");
					up(&inftl->mutex);
					res = 0;
					goto repeat;
				}
			}

			DEBUG(MTD_DEBUG_LEVEL2, "INFTL: read request "
				"completed OK\n");
			up(&inftl->mutex);
			goto repeat;
		} else if (req->cmd == WRITE) {
			DEBUG(MTD_DEBUG_LEVEL2, "INFTL: write request of 0x%x"
				" sectors @ %x (req->nr_sectors == %lx)\n",
				nsect, block, req->nr_sectors);
			for ( ; nsect > 0; nsect-- , block++, buffer += 512) {
				/* Read a single sector to req->buffer + (512 * i) */
				if (INFTL_writeblock(inftl, block, buffer)) {
					DEBUG(MTD_DEBUG_LEVEL1, "INFTL: "
						"write request failed\n");
					up(&inftl->mutex);
					res = 0;
					goto repeat;
				}
			}
			DEBUG(MTD_DEBUG_LEVEL2, "INFTL write request "
				"completed OK\n");
			up(&inftl->mutex);
			goto repeat;
		} else {
			DEBUG(MTD_DEBUG_LEVEL0, "INFTL: unknown request\n");
			up(&inftl->mutex);
			res = 0;
			goto repeat;
		}
	repeat: 
		DEBUG(MTD_DEBUG_LEVEL3, "INFTL: end_request(%d)\n", res);
		spin_lock_irq(&io_request_lock);
		end_request(res);
	}
}

static int inftl_open(struct inode *ip, struct file *fp)
{
	int inftlnum = MINOR(ip->i_rdev) >> INFTL_PARTN_BITS;
	struct INFTLrecord *thisINFTL;
	thisINFTL = INFTLs[inftlnum];

	DEBUG(MTD_DEBUG_LEVEL2, "INFTL: INFTL_open()\n");

#ifdef CONFIG_KMOD
	if (!thisINFTL && inftlnum == 0) {
		request_module("docprobe");
		thisINFTL = INFTLs[inftlnum];
	}
#endif
	if (!thisINFTL) {
		DEBUG(MTD_DEBUG_LEVEL2, "INFTL: ENODEV, thisINFTL = %d, "
			"minor = %d, ip = %p, fp = %p\n", 
			inftlnum, ip->i_rdev, ip, fp);
		return -ENODEV;
	}

	thisINFTL->usecount++;
	if (!get_mtd_device(thisINFTL->mtd, -1))
		return -ENXIO;

	return 0;
}

static int inftl_release(struct inode *inode, struct file *fp)
{
	struct INFTLrecord *thisINFTL;

	thisINFTL = INFTLs[MINOR(inode->i_rdev) / 16];

	DEBUG(MTD_DEBUG_LEVEL2, "INFTL: INFTL_release()\n");

	if (thisINFTL->mtd->sync)
		thisINFTL->mtd->sync(thisINFTL->mtd);
	thisINFTL->usecount--;

	put_mtd_device(thisINFTL->mtd);

	return 0;
}

static struct block_device_operations inftl_fops = 
{
	.owner =	THIS_MODULE,
	.open =		inftl_open,
	.release =	inftl_release,
	.ioctl = 	inftl_ioctl
};

/****************************************************************************
 *
 * Module stuff
 *
 ****************************************************************************/
                                                                                
static struct mtd_notifier inftl_notifier = {
	.add = INFTL_notify_add,
	.remove = INFTL_notify_remove
};
                                                                                
extern char inftlmountrev[];

static int __init init_inftl(void)
{
	int i;

	printk(KERN_INFO "INFTL: inftlcore.c $Revision: 1.2 $, "
		"inftlmount.c %s\n", inftlmountrev);

	if (register_blkdev(MAJOR_NR, "inftl", &inftl_fops)) {
		printk(KERN_WARNING "INFTL: unable to register INFTL block "
			"device on major %d\n", MAJOR_NR);
		return -EBUSY;
	}

	blk_init_queue(BLK_DEFAULT_QUEUE(MAJOR_NR), &inftl_request);

	/* set block size to 1kb each */
	for (i = 0; i < 256; i++)
		inftl_blocksizes[i] = 1024;
	blksize_size[MAJOR_NR] = inftl_blocksizes;

	add_gendisk(&inftl_gendisk);
	register_mtd_user(&inftl_notifier);
	return 0;
}

static void __exit cleanup_inftl(void)
{
	unregister_mtd_user(&inftl_notifier);
	unregister_blkdev(MAJOR_NR, "inftl");
	blk_cleanup_queue(BLK_DEFAULT_QUEUE(MAJOR_NR));
	del_gendisk(&inftl_gendisk);
}

module_init(init_inftl);
module_exit(cleanup_inftl);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Greg Ungerer <gerg@snapgear.com>, David Woodhouse <dwmw2@infradead.org>, Fabrice Bellard <fabrice.bellard@netgem.com> et al.");
MODULE_DESCRIPTION("Support code for Inverse Flash Translation Layer, used on M-Systems DiskOnChip 2000, Millennium and Millennium Plus");
