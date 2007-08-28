/*
 * linux/arch/e1nommu/kernel/setup.c
 *
 * Copyright (C) 2002-2003  George Thanos <george.thanos@gdt.gr>
 * 			    Yannis Mitsos <yannis.mitsos@gdt.gr>
 *
 * derived from:
 *  linux/arch/m68knommu/kernel/setup.c
 *
 *  Copyleft  ()) 2000       James D. Schettine {james@telos-systems.com}
 *  Copyright (C) 1999,2000  Greg Ungerer (gerg@uClinux.org)
 *  Copyright (C) 1998-2003  D. Jeff Dionne <jeff@uClinux.org>
 *  Copyright (C) 1998       Kenneth Albanowski <kjahds@kjahds.com>
 *  Copyright (C) 1995       Hamish Macdonald
 *  Copyright (C) 2000       Lineo Inc. (www.lineo.com) 
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
  
unsigned long rom_length;
unsigned long memory_start;
unsigned long memory_end;

struct task_struct *_current_task;

//char command_line[512]="root=/dev/ram0 init=/linuxrc rw";
//char command_line[512]="console=ttyS0,115200 devfs=dreg,digmod,dall root=/dev/rd/0 init=/linuxrc";
//char command_line[512]="ip=147.102.7.253::147.102.7.200:255.255.255.0:aceos:eth0:off root=/dev/nfs nfsroot=147.102.19.18:/hyperstone-root1 console=ttyS0,115200";
//char command_line[512]="console=ttyS0,115200 devfs=dreg,digmod,dall ip=:::::: root=/dev/nfs nfsroot=147.102.19.18:/hyperstone-root";
//char command_line[512]="console=ttyS0,115200 devfs=dreg,digmod,dall";
char command_line[512];

char saved_command_line[512];

void (*mach_sched_init) (void (*handler)(int, void *, struct pt_regs *)) = NULL;
void (*mach_tick)( void ) = NULL;
/* machine dependent keyboard functions */
int (*mach_keyb_init) (void) = NULL;
#ifdef __NF_NOT_DEF
int (*mach_kbdrate) (struct kbd_repeat *) = NULL;
#endif
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
void (*mach_reset)( void ) = NULL;
int (*mach_set_clock_mmss) (unsigned long) = NULL;
void (*mach_debug_init)(void) = NULL;
void (*mach_halt)( void ) = NULL;
void (*mach_power_off)( void ) = NULL;

//#define DEBUG	1

#ifdef CONFIG_E132XS
	#define CPU "Hyperstone E1-32XS"
#endif

#ifdef CONFIG_BLK_DEV_BLKMEM
#define CAT_ROMARRAY
#endif

extern int _stext, _etext, _sdata, _edata, _sbss, _ebss, _end;
extern int _ramstart, _ramend;

#ifdef CONFIG_BLK_DEV_INITRD
extern int _sinitrd, _einitrd;
#endif

extern unsigned long fExt_Osc;
extern int _sbss, __ramend;

void setup_arch(char **cmdline_p)
{
	int bootmap_size;

#undef ZERO_MEM
#ifdef ZERO_MEM
	int start_mem = (int)&_ebss;
	int end_mem = (int)&__ramend;
	memset((char*)&_ebss, 0, end_mem - start_mem);
	printk("ZERO MEM : 0x%x -> 0x%x\n", start_mem, end_mem );
#endif

#ifdef CAT_ROMARRAY
#if 0
	extern int __data_rom_start;
	extern int __data_start;
	int *romarray = (int *)((int) &__data_rom_start +
				      (int)&_edata - (int)&__data_start);

#endif
	int	 *romarray = (int *)(&_ebss);
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

	config_BSP(&command_line[0], sizeof(command_line));

	printk("\x0F\r\n\nuClinux/" CPU "\n");
	printk("Flat model support (C) 1998,1999 Kenneth Albanowski, D. Jeff Dionne\n");

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

#ifdef CONFIG_BLK_DEV_INITRD
	initrd_start = (unsigned long)&_sinitrd;
	initrd_end = (unsigned long)&_einitrd;
	printk("initrd starts at 0x%X, ends at 0x%X\n", initrd_start, initrd_end);
	if (initrd_end > memory_end) {
		printk("initrd extends beyond end of memory (0x%X), disabling\n", memory_end);
		initrd_start = 0;
	}
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

/*    clockfreq = (loops_per_jiffy*HZ)*16; */
    clockfreq = fExt_Osc * 1000;

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

/*
 *	Get CPU information for use by the procfs.
 */

static int show_cpuinfo(struct seq_file *m, void *v)
{
    char *cpu, *mmu, *fpu;
    u_long clockfreq;
	signed short mult_factor[8]={3, 2, 1, 0, -1, 0, 0, 0};
	unsigned long tmp;

    cpu = CPU;
    mmu = "none";
    fpu = "none";

	tmp =  fExt_Osc << mult_factor[((system_sh_regs->TPRValue >> 26) & 0x7)];
    clockfreq = tmp * 1000000;

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

/* whatever... copied from m68knommu */
static void *c_start(struct seq_file *m, loff_t *pos)
{ return (*pos < NR_CPUS) ? ((void *)0x12345678) : NULL; }
static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{ ++*pos; return c_start(m, pos); }
static void c_stop(struct seq_file *m, void *v)
{ ; }

struct seq_operations cpuinfo_op = {
	start:	c_start,
	next:	c_next,
	stop:	c_stop,
	show:	show_cpuinfo,
};

void arch_gettod(int *year, int *mon, int *day, int *hour,
		 int *min, int *sec)
{
	if (mach_gettod)
		mach_gettod(year, mon, day, hour, min, sec);
	else
		*year = *mon = *day = *hour = *min = *sec = 0;
}
