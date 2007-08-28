/*
 * linux/arch/e1nommu/platform/E132XS/config.c
 *
 * Copyright (C) 2002-2003 GDT,  Yannis Mitsos<yannis.mitsos@gdt.gr>
 *                               George Thanos<george.thanos@gdt.gr>
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/sched.h> /* for request_irq FIXME. Later we should change according to m68knommu implementation*/
#include <linux/irq.h>
#include <linux/interrupt.h>

#include <asm/setup.h>
#include <asm/system.h>
#include <asm/pgtable.h>
#include <asm/irq.h>
#include <asm/machdep.h>
#include <asm/io.h>
#include <asm/processor.h>

extern void st16550_serial_init(void);
extern void st16550_serial_set(unsigned long baud);

unsigned long fExt_Osc = CONFIG_HYPERSTONE_OSC_FREQ_MHZ; 	/* default assumption */

/* Since the FCR register is write only currently we do not
 * have any mean to know what was the initial value. Later we shall 
 * consider applying the same methodology  as the HyRTK using a segment
 * where all the write only registers are stored. Until then we initialize
 * the FCR in our own way.
 */

/* The TPR register is configured in such way that the internal clock is 
 * XTAL1 * 8 and the Timer prescaler is set to 0xA, so the precision of 
 * the interanl timer is 10MHz. 
 */
extern void timer_interrupt(int irq, void *dummy, struct pt_regs * regs);

static struct irqaction irq8  = { timer_interrupt, SA_INTERRUPT, 0, "timer", NULL, NULL};

void BSP_sched_init(void (*handler)(int, void *, struct pt_regs *))
{
	volatile long TPR_value, TCR_value;
	signed int ClockDivider[5] = {3, 2, 1, 0, -1};
	int return_value;
	TPR_value = 0x000a0000;
	
	TCR_value = (CONFIG_HYPERSTONE_OSC_FREQ_MHZ << ClockDivider[(0x7 & (TPR_value >> 26))]);  
	TCR_value = TCR_value / ((0xff &(TPR_value >> 16))+2);
	TCR_value = (TCR_value * 1000000 ) / 100;   
	
#ifdef DEBUG
	printk("BSP_sched_init: Frequency of external Oscillator = %lu MHz, computed TCR value %lu ticks\n", fExt_Osc ,TCR_value);
#endif

  /* setup irq handler before enabling the timer 
   * We cannot use the request_irq since the kmalloc
   * is not functioning at this time. So, we set the timer interrupt
   * by allocating an irqaction in our own */
	
	return_value = setup_irq(IRQ_TIMER ,&irq8);
	if (return_value){
		printk("Request for the Timer's IRQ failed!\n");
		mach_halt();
	}

	update_sh_reg(TPR_reg, TPR_value);

	asm volatile (" 
				FETCH   4	
				ORI		SR, 0x20
				MOVI	TR, 0x0
				ORI		SR, 0x20
				MOV		TCR, %1"
				: /* no outputs */
				: "l" (TPR_value), "l" (TCR_value)
				); 

	update_sh_reg_bit(FCR_reg, 23, 0); /* Enable the timer's interrupt */
}


void BSP_tick(void)
{
  /* FIXME Reset Timer by adding a constant value to the TCR. Should we change
   * it later to calculete the value according to the FCR ? */
		
	static unsigned long TCR_reload;

	asm volatile(
			"FETCH 4\n\t"
			"ORI SR, 0x20\n\t"
			"MOV %0, TR\n\t"
			"ADDI %0, 0x186A0\n\t"    // 10ms
//			"ADDI %0, 0xF4240\n\t"    // 100ms
//			"ADDI %0, 0x989680\n\t"  // 1 sec
//			"ADDI %0, 0x1312D00\n\t"  // 2 sec
//			"ADDI %0, 0x0BEBC200\n\t" // 20 sec
//			"ADDI %0, 0x1fffffff\n\t" // 20 sec
			"ORI SR, 0x20\n\t"
			"MOV TCR, %0\n\t"
			: "=l" (TCR_reload) 
			: "0" (TCR_reload) 
			);	
	
}

unsigned long BSP_gettimeoffset (void)
{
  return 0;
}

void BSP_gettod (int *yearp, int *monp, int *dayp,
		   int *hourp, int *minp, int *secp)
{
	*yearp = *monp = *dayp = *hourp = *minp = *secp = 0;
}

int BSP_set_clock_mmss (unsigned long nowtime)
{
  return 0;
}

void BSP_reset (void)
{
	cli();
/* FIXME */
	mach_halt();

}

/* The following function copies the values of the Write-only registers
 * from the location addressed via the G10. We should change it later
 * if we would like to boot directly from flash without the help of the 
 * ROM stub. (Yannis-FIXME) 
 */

shadow_regs_t system_regs;
shadow_regs_t *system_sh_regs; 

static void	BSP_copy_regs(void)
{
volatile unsigned long *base_addr;
unsigned char i;
	
	system_sh_regs = &system_regs;

	asm volatile("mov %0, G10"
					:"=l" (base_addr)
					:);
	for(i=0; i < 9; i++)
		memcpy((unsigned long *) system_sh_regs+i,(unsigned long *) base_addr+i, sizeof(*base_addr));
}

unsigned int io_periph[NR_CS];

void BSP_init_periphs(void)
{
	int i;

	for(i=0; i<NR_CS; i++){
		io_periph[i]= (SLOW_IO_ACCESS | i << 22);
	}
#ifdef CONFIG_HYPERSTONE_CS89X0
	io_periph[4] = (io_periph[4] | 1 << IOWait);
	/* In the Evaluation Board the Interrupt line for the Ethernet is active low. 
	 * So, we modify accordingly the FCR register */
	update_sh_reg(FCR_reg, 0x66ffFFFF);
#endif

#ifdef CONFIG_SERIAL_CONSOLE
	update_sh_reg_bit(FCR_reg, 25, 0);
	update_sh_reg_bit(FCR_reg, 29, 0);
#endif
}

#ifdef CONFIG_EMBEDDED_SERIAL_CONSOLE 
extern void st16550_serial_init(void);
extern void st16550_setup_console(void);
extern void st16550_serial_set(unsigned long baud);
#endif

void config_BSP(char *command, int len)
{

	memset(command, 0, len);

#ifdef CONFIG_HYPERSTONE_CS89X0
	strcat(command, "ip=" 
#ifdef CONFIG_IP_PNP_DHCP || CONFIG_IP_PNP_BOOTP
	":"
#else
	CONFIG_E132XS_BOARD_NET_BOARD_IP ":"
#endif 
	":" CONFIG_E132XS_BOARD_NET_DEFAULTGW ":" 
    CONFIG_E132XS_BOARD_NET_NETMASK ":" CONFIG_E132XS_BOARD_NET_HOSTNAME ":eth0:"
#ifdef CONFIG_IP_PNP_DHCP || CONFIG_IP_PNP_BOOTP
	"bootp ");
#else
	"off ");
#endif 
#endif

#ifdef CONFIG_ROOT_NFS
	strcat(command, "root=/dev/nfs rw nfsroot=" CONFIG_E132XS_BOARD_NET_NFS_ROOT " ");
#endif

#ifdef CONFIG_EXTRA_ARGS
	strcat(command, CONFIG_EXTRA_ARGS_VARIABLES);
#endif

	printk("\nE1-32XS evaluation board support (C) 2002 George Thanos and Yannis Mitsos.\n");

	BSP_copy_regs();

#ifdef CONFIG_EXTERNAL_DEVICES
	BSP_init_periphs();
#endif

#ifdef CONFIG_EMBEDDED_SERIAL_CONSOLE
	st16550_serial_init();
	st16550_serial_set(115200);
	st16550_setup_console();
#endif

  mach_sched_init      = BSP_sched_init;
  mach_tick            = BSP_tick; 
  mach_gettimeoffset   = BSP_gettimeoffset;
  mach_gettod          = BSP_gettod;
//  mach_hwclk           = NULL;
  mach_set_clock_mmss  = NULL;
  mach_reset           = BSP_reset;
  mach_trap_init       = NULL;
 // mach_debug_init      = NULL;
 
}
