/* Changes made by Akbar Hussain April 05, 2001  for FRIO */

/*
 *  linux/arch/m68knommu/kernel/setup.c
 *
 *  Copyleft  ()) 2000       James D. Schettine {james@telos-systems.com}
 *  Copyright (C) 1999,2000  Greg Ungerer (gerg@lineo.com)
 *  Copyright (C) 1998,1999  D. Jeff Dionne <jeff@lineo.ca>
 *  Copyright (C) 1998       Kenneth Albanowski <kjahds@kjahds.com>
 *  Copyright (C) 1995       Hamish Macdonald
 *  Copyright (C) 2000       Lineo Inc. (www.lineo.com) 
 *  Copyright (C) 2003	     Metrowerks
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

#define DEBUG 1
#ifdef CONFIG_BLACKFIN
	#define CPU "Blackfin"
#endif

#ifndef CPU
	#define	CPU "UNKOWN"
#endif

extern int _stext, _etext, _sdata, _edata, _sbss, _ebss, _end;
extern int _ramstart, _ramend;

void setup_arch(char **cmdline_p)
{
	int bootmap_size;

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
	// init_mm.brk = (unsigned long) &_end;
	
	config_BSP(&command_line[0], sizeof(command_line));

	printk("uClinux/" CPU "\n");

#ifdef CONFIG_BLACKFIN
#  ifdef CONFIG_BLACKFIN_EZKIT
	printk("Blackfin BF533 support by BuyWays (www.buyways.nl)\n");
#  else
	printk("Blackfin support by Metrowerks (www.metrowerks.com)\n");
#  endif
#endif

#ifdef DEBUG
	printk("Memory map:\n  text = 0x%06x-0x%06x\n  data = 0x%06x-0x%06x\n  bss  = 0x%06x-0x%06x\n  romfs= 0x%06x-0x%06x\n  mem  = 0x%06x-0x%06x\n  stack= 0x%06x-0x%06x\n",
		(int)&_stext,(int)&_etext,(int)&_sdata,(int)&_edata,
		(int)&_sbss,(int)&_ebss,(int)&_ebss,(int)memory_start,
		(int)memory_start,(int)memory_end,(int)memory_end,
		(int)_ramend);
#endif

	init_task.mm->start_code = (unsigned long) &_stext;
	init_task.mm->end_code = (unsigned long) &_etext;
	init_task.mm->end_data = (unsigned long) &_edata;
	init_task.mm->brk = (unsigned long) &_end;

	ROOT_DEV = MKDEV(BLKMEM_MAJOR,0);

	/* Keep a copy of command line */
	*cmdline_p = &command_line[0];
	memcpy(saved_command_line, command_line, sizeof(saved_command_line));
	saved_command_line[sizeof(saved_command_line)-1] = 0;

#ifdef DEBUG
	if (strlen(*cmdline_p)) 
		printk("Command line: '%s'\n", *cmdline_p);
#endif
	
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
}

int get_cpuinfo(char * buffer)
{
    char *cpu, *mmu, *fpu;
    u_long clockfreq;

    cpu = CPU;
    mmu = "none";
    fpu = "none";

#ifdef CONFIG_BLACKFIN
    clockfreq = (loops_per_jiffy*HZ)*3;
#else
    clockfreq = (loops_per_jiffy*HZ)*16; 
#endif

    return(sprintf(buffer, "CPU:\t\t%s\n"
		   "MMU:\t\t%s\n"
		   "FPU:\t\t%s\n"
		   "Clocking:\t%lu.%1luMHz\n"
		   "BogoMips:\t%lu.%02lu\n"
		   "Calibration:\t%lu loops\n",
		   cpu, mmu, fpu,
		   clockfreq/1000000,(clockfreq/100000)%10,
		   (loops_per_jiffy*HZ)/500000,((loops_per_jiffy*HZ)/5000)%100,
		   (loops_per_jiffy*HZ)));

}

void arch_gettod(int *year, int *mon, int *day, int *hour,
		 int *min, int *sec)
{
	if (mach_gettod)   /* depend on arch, should provide mach_gettod */
		mach_gettod(year, mon, day, hour, min, sec);
	else
		*year = *mon = *day = *hour = *min = *sec = 0;
}
