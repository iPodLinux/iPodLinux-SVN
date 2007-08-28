/*
 *  linux/arch/m68knommu/kernel/setup.c
 *
 *  Copyleft  ()) 2000       James D. Schettine {james@telos-systems.com}
 *  Copyright (C) 1999-2003  Greg Ungerer (gerg@snapgear.com)
 *  Copyright (C) 1998,1999  D. Jeff Dionne <jeff@uClinux.org>
 *  Copyright (C) 1998       Kenneth Albanowski <kjahds@kjahds.com>
 *  Copyright (C) 1995       Hamish Macdonald
 *  Copyright (C) 2000       Lineo Inc. (www.lineo.com) 
 *  Copyright (C) 2001 	     Lineo, Inc. <www.lineo.com>
 *
 */

/*
 * This file handles the architecture-dependent parts of system setup
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/fb.h>
#include <linux/console.h>
#include <linux/genhd.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/major.h>
#include <linux/bootmem.h>
#include <linux/seq_file.h>

#include <asm/setup.h>
#include <asm/irq.h>
#include <asm/machdep.h>

#ifdef CONFIG_BLK_DEV_INITRD
#include <linux/blk.h>
#include <asm/pgtable.h>
#endif

#ifdef CONFIG_CONSOLE
extern struct consw *conswitchp;
#ifdef CONFIG_FRAMEBUFFER
extern struct consw fb_con;
#endif
#endif

#if defined (CONFIG_M68360)
#include<asm/m68360.h>  //Get the definition of QUICC type
extern void m360_cpm_reset(void);
extern QUICC *pquicc;
extern struct console sercons;

// Mask to select if the PLL prescaler is enabled.
#define MCU_PREEN   ((unsigned short)(0x0001 << 13))

#if defined(CONFIG_UCQUICC)
#define OSCILLATOR  (unsigned long int)33000000
#endif

unsigned long int system_clock;
#endif

#if defined(CONFIG_M68VZ328)
#include <asm/MC68VZ328.h>
#endif
  
unsigned long rom_length;
unsigned long memory_start;
unsigned long memory_end;

struct task_struct *_current_task;

char command_line[512];
char saved_command_line[512];

/* setup some dummy routines */
static void dummy_waitbut(void)
{
}

void (*mach_sched_init) (void (*handler)(int, void *, struct pt_regs *)) = NULL;
void (*mach_tick)( void ) = NULL;
/* machine dependent keyboard functions */
int (*mach_keyb_init) (void) = NULL;
int (*mach_kbdrate) (struct kbd_repeat *) = NULL;
void (*mach_kbd_leds) (unsigned int) = NULL;
/* machine dependent irq functions */
void (*mach_init_IRQ) (void) = NULL;
void (*(*mach_default_handler)[]) (int, void *, struct pt_regs *) = NULL;
int (*mach_request_irq) (unsigned int, void (*)(int, void *, struct pt_regs *),
                         unsigned long, const char *, void *);
void (*mach_free_irq) (unsigned int irq, void *dev_id) = NULL;
void (*mach_enable_irq) (unsigned int) = NULL;
void (*mach_disable_irq) (unsigned int) = NULL;
int (*mach_get_irq_list) (char *) = NULL;
void (*mach_process_int) (int irq, struct pt_regs *fp) = NULL;
void (*mach_trap_init) (void);
/* machine dependent timer functions */
unsigned long (*mach_gettimeoffset) (void) = NULL;
void (*mach_gettod) (int*, int*, int*, int*, int*, int*) = NULL;
int (*mach_hwclk) (int, struct hwclk_time*) = NULL;
int (*mach_set_clock_mmss) (unsigned long) = NULL;
void (*mach_mksound)( unsigned int count, unsigned int ticks ) = NULL;
void (*mach_reset)( void ) = NULL;
void (*waitbut)(void) = dummy_waitbut;
void (*mach_debug_init)(void) = NULL;
void (*mach_halt)( void ) = NULL;
void (*mach_power_off)( void ) = NULL;


#ifdef CONFIG_M68000
	#define CPU "MC68000"
#endif
#ifdef CONFIG_M68328
	#define CPU "MC68328"
#endif
#ifdef CONFIG_M68EZ328
	#define CPU "MC68EZ328"
#endif
#ifdef CONFIG_M68VZ328
	#define CPU "MC68VZ328"
#endif
#ifdef CONFIG_M68332
	#define CPU "MC68332"
#endif
#ifdef CONFIG_M68360
	#define CPU "MC68360"
#endif
#if defined(CONFIG_M5206)
	#define	CPU "COLDFIRE(m5206)"
#endif
#if defined(CONFIG_M5206e)
	#define	CPU "COLDFIRE(m5206e)"
#endif
#if defined(CONFIG_M5249)
	#define CPU "COLDFIRE(m5249)"
#endif
#if defined(CONFIG_M5272)
	#define CPU "COLDFIRE(m5272)"
#endif
#if defined(CONFIG_M5280)
	#define CPU "COLDFIRE(m5280)"
#endif
#if defined(CONFIG_M5282)
	#define CPU "COLDFIRE(m5282)"
#endif
#if defined(CONFIG_M5307)
	#define	CPU "COLDFIRE(m5307)"
#endif
#if defined(CONFIG_M5407)
	#define	CPU "COLDFIRE(m5407)"
#endif
#ifndef CPU
	#define	CPU "UNKOWN"
#endif

/* (es) */
/* note: why is this defined here?  the must be a better place to put this */
#if defined( CONFIG_TELOS) || defined( CONFIG_UCDIMM ) || defined( CONFIG_UCSIMM ) || defined(CONFIG_DRAGEN2) || (defined( CONFIG_PILOT ) && defined( CONFIG_M68328 ))
#define CAT_ROMARRAY
#endif
/* (/es) */

extern int _stext, _etext, _sdata, _edata, _sbss, _ebss, _end;
extern int _ramstart, _ramend;

void setup_arch(char **cmdline_p)
{
	int bootmap_size;

#if defined(CAT_ROMARRAY) && defined(DEBUG)
	extern int __data_rom_start;
	extern int __data_start;
	int *romarray = (int *)((int) &__data_rom_start +
			      (int)&_edata - (int)&__data_start);
#endif

#if defined(CONFIG_CHR_DEV_FLASH) || defined(CONFIG_BLK_DEV_FLASH)
	/* we need to initialize the Flashrom device here since we might
	 * do things with flash early on in the boot
	 */
	flash_probe();
#endif

	memory_start = PAGE_ALIGN(_ramstart);
	memory_end = _ramend; /* by now the stack is part of the init task */

	init_mm.start_code = (unsigned long) &_stext;
	init_mm.end_code = (unsigned long) &_etext;
	init_mm.end_data = (unsigned long) &_edata;
#if 0 /* DAVIDM - don't set brk just incase someone decides to use it */
	init_mm.brk = (unsigned long) &_end;
#else
	init_mm.brk = (unsigned long) 0; 
#endif


#if defined (CONFIG_M68360)
    m360_cpm_reset();

   /* Calculate the real system clock value. */
   {
       unsigned int local_pllcr = (unsigned int)(pquicc->sim_pllcr);
       if( local_pllcr & MCU_PREEN ) // If the prescaler is dividing by 128
       {
           int mf = (int)(pquicc->sim_pllcr & 0x0fff);
           system_clock = (OSCILLATOR / 128) * (mf + 1);
       }
       else
       {
           int mf = (int)(pquicc->sim_pllcr & 0x0fff);
           system_clock = (OSCILLATOR) * (mf + 1);
       }
   }

   /* Setup SMC 2 as a serial console */
   serial_console_setup(&sercons, NULL);
   console_360_init(0, 0);
#endif

	config_BSP(&command_line[0], sizeof(command_line));

	printk("\r\nuClinux/" CPU "\n");

#ifdef CONFIG_UCDIMM
	printk("uCdimm by Arcturus Networks, Inc. <www.arcturusnetworks.com>\n");
#endif
#ifdef CONFIG_COLDFIRE
	printk("COLDFIRE port done by Greg Ungerer, gerg@snapgear.com\n");
#ifdef CONFIG_M5307
	printk("Modified for M5307 by Dave Miller, dmiller@intellistor.com\n");
#endif
#ifdef CONFIG_ELITE
	printk("Modified for M5206eLITE by Rob Scott, rscott@mtrob.fdns.net\n");
#endif  
#ifdef CONFIG_TELOS
	printk("Modified for Omnia ToolVox by James D. Schettine, james@telos-systems.com\n");
#endif
#endif
	printk("Flat model support (C) 1998,1999 Kenneth Albanowski, D. Jeff Dionne\n");

#if defined( CONFIG_PILOT ) && defined( CONFIG_M68328 )
	printk("TRG SuperPilot FLASH card support <info@trgnet.com>\n");
#endif

#if defined( CONFIG_PILOT ) && defined( CONFIG_M68EZ328 )
	printk("PalmV support by <jeff@uClinux.org>\n");
#endif

#ifdef CONFIG_M68EZ328ADS
	printk("M68EZ328ADS board support (C) 1999 Vladimir Gurevich <vgurevic@cisco.com>\n");
#endif

#ifdef CONFIG_ALMA_ANS
	printk("Alma Electronics board support (C) 1999 Vladimir Gurevich <vgurevic@cisco.com>\n");
#endif
#if defined (CONFIG_M68360)
    printk("QUICC port done by SED Systems <hamilton@sedsystems.ca>,\n");
    printk("based on 2.0.38 port by <mleslie@ArcturusNetworks.com>.\n");
#endif
#ifdef CONFIG_DRAGEN2
	printk("DragonEngine II board support by Georges Menie\n");
#endif

#ifdef CONFIG_CWEZ328
	printk("cwez328 board support by 2002 Andrew Ip <aip@cwlinux.com> and Inky Lung <ilung@cwlinux.com>\n");
#endif

#ifdef CONFIG_CWVZ328
	printk("cwvz328 board support by 2002 Andrew Ip <aip@cwlinux.com> and Inky Lung <ilung@cwlinux.com>\n");
#endif

#ifdef DEBUG
	printk("KERNEL -> TEXT=0x%06x-0x%06x DATA=0x%06x-0x%06x "
		"BSS=0x%06x-0x%06x\n", (int) &_stext, (int) &_etext,
		(int) &_sdata, (int) &_edata,
		(int) &_sbss, (int) &_ebss);
	printk("KERNEL -> ROMFS=0x%06x-0x%06x MEM=0x%06x-0x%06x "
		"STACK=0x%06x-0x%06x\n",
#ifdef CAT_ROMARRAY
	       (int) romarray, ((int) romarray) + romarray[2],
#else
	       (int) &_ebss, (int) memory_start,
#endif
		(int) memory_start, (int) memory_end,
		(int) memory_end, (int) _ramend);
#endif

#ifdef CONFIG_BLK_DEV_BLKMEM
	ROOT_DEV = MKDEV(BLKMEM_MAJOR,0);
#endif

	/* Keep a copy of command line */
	*cmdline_p = &command_line[0];
	memcpy(saved_command_line, command_line, sizeof(saved_command_line));
	saved_command_line[sizeof(saved_command_line)-1] = 0;

#ifdef DEBUG
	if (strlen(*cmdline_p)) 
		printk("Command line: '%s'\n", *cmdline_p);
#endif
	/*rom_length = (unsigned long)&_flashend - (unsigned long)&_romvec;*/
	
#ifdef CONFIG_CONSOLE
#ifdef CONFIG_FRAMEBUFFER
	conswitchp = &fb_con;
#else
	conswitchp = 0;
#endif
#endif

	/*
	 * give all the memory to the bootmap allocator,  tell it to put the
	 * boot mem_map at the start of memory
	 */
	bootmap_size = init_bootmem_node(
			NODE_DATA(0),
			memory_start >> PAGE_SHIFT, /* map goes here */
			PAGE_OFFSET >> PAGE_SHIFT,	/* 0 on coldfire */
			memory_end >> PAGE_SHIFT);
	/*
	 * free the usable memory,  we have to make sure we do not free
	 * the bootmem bitmap so we then reserve it after freeing it :-)
	 */
	free_bootmem(memory_start, memory_end - memory_start);
	reserve_bootmem(memory_start, bootmap_size);
	/*
	 * get kmalloc into gear
	 */
	paging_init();
#ifdef DEBUG
	printk("Done setup_arch\n");
#endif

}

int get_cpuinfo(char * buffer)
{
    char *cpu, *mmu, *fpu;
    u_long clockfreq;

    cpu = CPU;
    mmu = "none";
    fpu = "none";

#if defined(CONFIG_COLDFIRE)
    clockfreq = (loops_per_jiffy*HZ)*3;
#elif defined(CONFIG_DRAGONIXVZ)
    clockfreq = (loops_per_jiffy*HZ)*29;
#elif defined(CONFIG_UCDIMM)
    {
      unsigned short p, q;
      unsigned short prot;
      unsigned short sysclk_sel, presc1, presc2;

      p    = PLLFSR & PLLFSR_PC_MASK;
      q    = (PLLFSR & PLLFSR_QC_MASK) >> PLLFSR_QC_SHIFT;
      prot = (PLLFSR & PLLFSR_PROT)? 1 : 0;

      presc1 = (PLLCR & PLLCR_PRESC1) ? 1 : 0;
      presc2 = (PLLCR & PLLCR_PRESC2) ? 1 : 0;
      sysclk_sel = (PLLCR & PLLCR_SYSCLK_SEL_MASK) >> PLLCR_SYSCLK_SEL_SHIFT;

      clockfreq = 2*(14*(p+1) + q+1) * 32768;
      clockfreq = (clockfreq >> presc1) >> presc2;
    }
#else
    clockfreq = (loops_per_jiffy*HZ)*16;
#endif

    sprintf(buffer, "CPU:\t\t%s\n"
		   "MMU:\t\t%s\n"
		   "FPU:\t\t%s\n"
		   "Clocking:\t%lu.%1luMHz\n"
		   "BogoMips:\t%lu.%02lu\n"
		   "Calibration:\t%lu loops\n",
		   cpu, mmu, fpu,
		   clockfreq/1000000,(clockfreq/100000)%10,
		   (loops_per_jiffy*HZ)/500000,((loops_per_jiffy*HZ)/5000)%100,
	    (loops_per_jiffy*HZ));

    return (buffer);
}

/*
 *	Get CPU information for use by the procfs.
 */

static int show_cpuinfo(struct seq_file *m, void *v)
{
    char *cpu, *mmu, *fpu;
    u_long clockfreq;

    cpu = CPU;
    mmu = "none";
    fpu = "none";

#if defined(CONFIG_COLDFIRE)
    clockfreq = (loops_per_jiffy*HZ)*3;
#elif defined(CONFIG_DRAGONIXVZ)
    clockfreq = (loops_per_jiffy*HZ)*29;
#elif defined(CONFIG_UCDIMM)
    {
      unsigned short p, q;
      unsigned short prot;
      unsigned short sysclk_sel, presc1, presc2;

      p    = PLLFSR & PLLFSR_PC_MASK;
      q    = (PLLFSR & PLLFSR_QC_MASK) >> PLLFSR_QC_SHIFT;
      prot = (PLLFSR & PLLFSR_PROT)? 1 : 0;

      presc1 = (PLLCR & PLLCR_PRESC1) ? 1 : 0;
      presc2 = (PLLCR & PLLCR_PRESC2) ? 1 : 0;
      sysclk_sel = (PLLCR & PLLCR_SYSCLK_SEL_MASK) >> PLLCR_SYSCLK_SEL_SHIFT;

      clockfreq = 2*(14*(p+1) + q+1) * 32768;
      clockfreq = (clockfreq >> presc1) >> presc2;
    }
#else
    clockfreq = (loops_per_jiffy*HZ)*16;
#endif

    seq_printf(m, "CPU:\t\t%s\n"
		   "MMU:\t\t%s\n"
		   "FPU:\t\t%s\n"
		   "Clocking:\t%lu.%1luMHz\n"
		   "BogoMips:\t%lu.%02lu\n"
		   "Calibration:\t%lu loops\n",
		   cpu, mmu, fpu,
		   clockfreq/1000000,(clockfreq/100000)%10,
		   (loops_per_jiffy*HZ)/500000,((loops_per_jiffy*HZ)/5000)%100,
		   (loops_per_jiffy*HZ));

	return 0;
}

static void *c_start(struct seq_file *m, loff_t *pos)
{
	return *pos < NR_CPUS ? ((void *) 0x12345678) : NULL;
}

static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return c_start(m, pos);
}

static void c_stop(struct seq_file *m, void *v)
{
}

struct seq_operations cpuinfo_op = {
	start:	c_start,
	next:	c_next,
	stop:	c_stop,
	show:	show_cpuinfo,
};

void arch_gettod(int *year, int *mon, int *day, int *hour,
		 int *min, int *sec)
{
	*year = *mon = *day = *hour = *min = *sec = 0;
	if (mach_gettod)
		mach_gettod(year, mon, day, hour, min, sec);
}


