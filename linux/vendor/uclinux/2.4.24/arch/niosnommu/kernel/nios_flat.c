#include <linux/module.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/flat.h>
#include <linux/elf.h>

#include <asm/flat.h>

#define RELOC_OK            0
#define RELOC_UNSUPPORTED  -1
#define RELOC_OVERFLOW     -2
#define RELOC_BAD_SECTION  -3
#define RELOC_UNKNOWN_TYPE -4

/* temporary turning offthe trace */
#undef TRACE
#define TRACE 0
#undef TRACE_MAX
#define TRACE_MAX 1
static int do_nios32_reloc( reloc_struct *pReloc, unsigned long entry )
{
  	int             status = RELOC_OK;

	char           *pSrcSect; // SrcSect is section of the stuff being referenced. Def'n as char * for easy ptr. arithmetic
	char           *pDstSect; // DstSect is the section of the stuff that needs to be fixed up.
        unsigned short *pDst;     // ptr to the instruction
	unsigned short  insn;  	  // The 16-bit instruction
	unsigned long   value;    // The src address value

	// Reduce code size: Dereference variables that are used in all case blocks.
	signed long     addend    = pReloc->addend;
	signed long     srcOffset = pReloc->src_offset;
	signed long     dstOffset = pReloc->offset;
#if TRACE >= TRACE_MAX
	char           *srcSectName, *dstSectName, *relocName;
#endif

	/* Setup the DstSect. */
	switch ( pReloc->dest_section )
	{
	    case FLAT_RELOC_TYPE_TEXT :
		pDstSect = (char *)(current->mm->start_code);
#if TRACE >= TRACE_MAX
		dstSectName = ".text";
#endif
		break;

	    case FLAT_RELOC_TYPE_DATA :
		pDstSect = (char *)(current->mm->start_data);
#if TRACE >= TRACE_MAX
		dstSectName = ".data";
#endif
		break;

	    case FLAT_RELOC_TYPE_BSS:
		pDstSect = (char *)(current->mm->end_data);
#if TRACE >= TRACE_MAX
		dstSectName = ".bss";
#endif
		break;

  	    default :
		return RELOC_BAD_SECTION;
	}


	/* Setup the SrcSect. */
	switch ( pReloc->src_section )
	{
	    case FLAT_RELOC_TYPE_TEXT :
	        pSrcSect = (char *)(current->mm->start_code);
#if TRACE >= TRACE_MAX
		srcSectName = ".text";
#endif
		break;

	    case FLAT_RELOC_TYPE_DATA :
		pSrcSect = (char *)(current->mm->start_data);
#if TRACE >= TRACE_MAX
		srcSectName = ".data";
#endif
		break;

	    case FLAT_RELOC_TYPE_BSS:
		pSrcSect = (char *)(current->mm->end_data);
#if TRACE >= TRACE_MAX
		srcSectName = ".bss";
#endif
		break;

  	    default :
		return RELOC_BAD_SECTION;
	}


	dstOffset -= entry;
        pDst       = (unsigned short *)(pDstSect + dstOffset);
	value      = (unsigned long)(pSrcSect + srcOffset);
	insn       = *pDst;


  	switch( pReloc->reloc_type )
	{
	    case R_NIOS_NONE :                          // A no-op reloc (should not exist).
#if TRACE >= TRACE_MAX
	        relocName = "R_NIOS_NONE";
#endif
	  	status =  -1;
		break;

	    case R_NIOS_32 :                            // A 32 bit absolute relocation.
#if TRACE >= TRACE_MAX
	        relocName = "R_NIOS_32";
#endif
		*(long *)pDst = value + addend;
		break;

	    case R_NIOS_LO16_LO5 :                      // A LO-16 5 bit absolute relocation.
#if TRACE >= TRACE_MAX
	        relocName = "R_NIOS_LO16_LO5";
#endif
	        value += addend;
		value  = (value & 0x001f);
		insn   = ((insn & 0xfc1f) | (value << 5));
		*pDst  = insn;
		break;

  	    case R_NIOS_LO16_HI11 :                     // A LO-16 top 11 bit absolute relocation.
#if TRACE >= TRACE_MAX
	        relocName = "R_NIOS_LO16_HI11";
#endif
                value += addend;
		value  = ((value & 0xffff) >> 5);
		insn   = (insn & 0xf800) | value;
		*pDst  = insn;
		break;


	    case R_NIOS_HI16_LO5 :                      // A HI-16 5 bit absolute relocation.
#if TRACE >= TRACE_MAX
	        relocName = "R_NIOS_HI16_LO5";
#endif
                value  += addend;
		value   = ((value >> 16) & 0x001f);
		value <<= 5;
		insn    = ((insn & 0xfc1f) | value);
		*pDst   = insn;
		break;

	    case R_NIOS_HI16_HI11 :                     // A HI-16 top 11 bit absolute relocation.
#if TRACE >= TRACE_MAX
	        relocName = "R_NIOS_HI16_HI11";
#endif
	        value += addend;
		value  = (value >> 21);
		insn   = ((insn & 0xf800) | value);
		*pDst  = insn;
		break;

	    case R_NIOS_PCREL6 :                        // A 6 bit relative relocation.
#if TRACE >= TRACE_MAX
	        relocName = "R_NIOS_PCREL6";
#endif
	        value  -= (unsigned long)pDst;
		value  += addend;
		value  -= 4;
		value >>= 2;

		if ( (long)value > 0x3F || (long)value < 0 )
		    return -2;  // overflow

		value &= 0x3f;
		insn   = insn & 0xf800;
		insn   = insn | (value << 5);
		*pDst  = insn;
		break;


	    case R_NIOS_PCREL8 :                        // An 8 bit relative relocation.
#if TRACE >= TRACE_MAX
	        relocName = "R_NIOS_PCREL8";
#endif
	        value  -= (unsigned long)pDst;
		value  += addend;
		value  -= 2;
		value >>= 1;
		if ((long) value > 0xff || (long) value < 0)
		{
		    status = RELOC_OVERFLOW;
		    break;
		}

		insn  = insn & 0x701f;
		insn  = insn | (value << 5);
		*pDst = insn;
		break;

	    case R_NIOS_PCREL11 :                       // An 11 bit relative relocation.    
#if TRACE >= TRACE_MAX
	        relocName = "R_NIOS_PCREL11";
#endif
	        value -= (unsigned long)pDst;
		value += addend;
		/* insn uses bottom 11 bits of relocation value times 2 */
		value = (value & 0x7ff) >> 1;

		if ((long) value > 0x7ff)
		{
		    status = RELOC_OVERFLOW;
		    break;
		}

		insn  = insn & 0xf800;
		insn  = insn | value;
		*pDst = insn;
		break;

	    case R_NIOS_16 :                             // A 16 bit absolute relocation. (Should not see these here).
#if TRACE >= TRACE_MAX
	        relocName = "R_NIOS_16";
#endif
	        value  += addend;
		value >>= 1;
		*pDst   = (unsigned short)value;
		break;


	    case R_NIOS_H_LO5 :                         // Low 5-bits of absolute relocation in halfwords.
#if TRACE >= TRACE_MAX
	        relocName = "R_NIOS_H_LO5";
#endif
	        value += addend;
		value  = ((value >> 1) & 0x001f);
		insn   = ((insn & 0xfc1f) | (value << 5));
		*pDst  = insn;
		break;


	    case R_NIOS_H_HI11 :                        // Top 11 bits of 16-bit absolute relocation in halfwords.
#if TRACE >= TRACE_MAX
	        relocName = "R_NIOS_H_HI11";
#endif
	        value += addend;
		value  = ((value & 0x1fffe) >> 6);
                insn   = (insn & 0xf800) | value;
		*pDst  = insn;
		break;


	    case R_NIOS_H_XLO5 :                        // Low 5 bits of top 16-bits of 32-bit absolute relocation in halfwords.
#if TRACE >= TRACE_MAX
	        relocName = "R_NIOS_H_XLO5";
#endif
	        value  += addend;
	        value   = ((value >> 17) & 0x001f);
		value <<= 5;
		insn    = ((insn & 0xfc1f) | value);
		*pDst   = insn;
		break;


	    case R_NIOS_H_XHI11 :                       // Top 11 bits of top 16-bits of 32-bit absolute relocation in halfwords.
#if TRACE >= TRACE_MAX
	      relocName = "R_NIOS_H_XHI11";
#endif
	        value  += addend;
		value >>= 22;
		insn    = ((insn & 0xf800) | value);
		*pDst   = insn;
		break;


	    case R_NIOS_H_16 :                          // Half-word @h value.
#if TRACE >= TRACE_MAX
	        relocName = "R_NIOS_H_XH111";
#endif
	        value  += addend;
		value >>= 1;

		if (value > 0xffff)
		{
		    status = RELOC_OVERFLOW;
		    break;
		}

		*pDst = insn;
		break;


	    case R_NIOS_H_32 :                          // Word @h value.
#if TRACE >= TRACE_MAX
	        relocName = "R_NIOS_H_32";
#endif
	        value  += addend;
	        value >>= 1;
	        *(unsigned long *)pDst = value;
	        break;


	    case R_NIOS_GNU_VTINHERIT :                 // GNU extension to record C++ vtable hierarchy.
#if TRACE >= TRACE_MAX
	        relocName = "R_NIOS_GNU_VTINHERIT";
#endif
		status = RELOC_UNSUPPORTED;
		break;


 	    case R_NIOS_GNU_VTENTRY :                   // GNU extension to record C++ vtable member usage.
#if TRACE >= TRACE_MAX
	        relocName = "R_NIOS_GNU_VTENTRY";
#endif
	        status = RELOC_UNSUPPORTED;
		break;
  

	    default :
#if TRACE >= TRACE_MAX
	        relocName = "UNKNOWN RELOC TYPE";
#endif
	        status = RELOC_UNKNOWN_TYPE;
	        break;
	}

#if TRACE >= TRACE_MAX
	printk( "Reloc[%ul] dst_section = %s, dst_offset = 0x%08lX (%ld), reloc_type %s\n"
                "           src_section = %s, src_offset = 0x%08lX (%ld), addend = 0x%08lX (%ld)\n",
		0, dstSectName, dstOffset, dstOffset, relocName,
                srcSectName, srcOffset, srcOffset, addend, addend );
#endif
	return status;
}


/**************************************************/
/* reloc_struct's offset indicated where relative */
/* to mm->start_data to adjust an address value.  */
/* flat_struct's type indicates the section in    */
/* which the value resides: Add the appropriate   */
/* adjustment based upon the section type. Note:  */
/* This behaviour is appropriate for the original */
/* binfmt_flat format for 68K-based systems. The  */
/* Nios uses the extended format where the type   */
/* field indicates the extended format and the    */
/* reloc_type field indicated the kind of reloc.  */
/* to perform.                                    */
/**************************************************/
void nios_old_reloc(reloc_struct *r, unsigned long entry)
{
  	long	       offset;
  	unsigned long *pLong;
	void          *pVoid;
	int            i;
	int            status;

	/* Do the endian flip. */
	for ( i = sizeof(reloc_struct) / sizeof(unsigned long), pLong = (unsigned long *)r; i; i--, pLong++ )
		*pLong = htonl( *pLong );


	offset = r->offset;
	pVoid = (void *)current->mm->start_data + offset;	// Points at stuff to relocate: start_data is the origin


#ifdef DEBUG0 /* wentao */
	printk("TEXTSEG=%x, DATASEG=%x, BSSSEG=%x\n",
		current->mm->start_code,
		current->mm->start_data,
		current->mm->end_data);

	printk("Relocation type = %d, offset = %d\n", r->type, r->offset);
#endif

	
	switch (r->type) {
	case FLAT_RELOC_TYPE_TEXT:
	  *(long *)pVoid += current->mm->start_code;
	  break;
	case FLAT_RELOC_TYPE_DATA:
	  *(long *)pVoid += current->mm->start_data;
	  break;
	case FLAT_RELOC_TYPE_BSS:
	  *(long *)pVoid += current->mm->end_data;
	  break;

	case FLAT_RELOC_EXTENDED:
	  status = do_nios32_reloc( r, entry );
#if TRACE >= TRACE_MAX
	  switch ( status )
          {
	      case RELOC_OK          :                                                 break;
  	      case RELOC_OVERFLOW    :  printk( "reloc overflow\n" );                  break;
	      case RELOC_UNSUPPORTED :  printk( "reloc unsupported type\n" );          break;
	      case RELOC_UNKNOWN_TYPE:  printk( "reloc unknown type\n" );              break;
	      case RELOC_BAD_SECTION :  printk( "reloc bad section\n" );               break;
    	      default                :  printk( "reloc unknown status %d\n", status ); break;
	  }
#endif
	  break;


	default:
	  printk("Unknown relocation\n");
	}
}		