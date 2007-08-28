/* 
 * 68328 Power Management Routines
 * 
 *  Copyright 2002 Daniel Potts <danielp@cse.unsw.edu.au>
 *                 Embedded Systems, Co., LTD, Korea
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

/*
 *  Debug macros 
 */
#define DEBUG 1
#ifdef DEBUG
#  define DPRINTK(fmt, args...)	printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#endif


#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/sysctl.h>

#include <linux/acpi.h> /* WARNING: included for CTL_ACPI and ACPI_S1_SLP_TYP */

#ifdef CONFIG_M68328
#include <asm/MC68328.h>
#endif

#ifdef CONFIG_M68EZ328
#include <asm/MC68EZ328.h>
#endif

#ifdef CONFIG_M68VZ328
#include <asm/MC68VZ328.h>
#endif

#include <linux/kmod.h>
#include <linux/interrupt.h> /* for interrupt stuff (that shouldn't be here)*/
#include <asm/irq.h>

static unsigned long sleep_irq_mask = 0L; /* irqs to enable while in sleep mode */

#if defined (CONFIG_68328_SERIAL)
void shutdown_console(void);
void startup_console(void);
#endif

void set_sleep_mask(unsigned long mask)
{
	sleep_irq_mask = mask;
}

static int cpu_suspend(void)
{
	unsigned long flags;
	unsigned long imr_flags;

	save_flags(flags);
	cli();
	imr_flags = IMR;

#if defined (CONFIG_68328_SERIAL)
	/* Console is shutdown here as a special device,
	 * to ensure that the kernel will not hang trying to write to console.
	 */
	shutdown_console();
#endif

	/* write out sleep enabled interrupts */
	IMR = ~(sleep_irq_mask);    

#if defined (CONFIG_PM_DOZE_ONLY)
	PCTRL = PCTRL_PCEN;
#else
	PLLCR |= PLLCR_DISPLL;
	__asm__("stop #0x2000" : : : "cc");
#endif

	/* Resume normal operation */

#if defined (CONFIG_68328_SERIAL)
	startup_console();
#endif

	IMR = imr_flags;
	restore_flags(flags);

	return 0;
}

#if defined (CONFIG_PM_HELPER)

static char pm_helper_path[128] = "/sbin/pm_helper";

static void run_pm_helper(pm_request_t action)
{
	int r;
	char *argv[3], *envp[3];

	if (!pm_helper_path[0])
		return;
	if (!current->fs->root)
		return;
	if (action != PM_SUSPEND && action != PM_RESUME)
		return;

	argv[0] = pm_helper_path;
	argv[1] = (action == PM_RESUME ? "resume" : "suspend");
	argv[2] = 0;

	/* minimal command environment */
	envp[0] = "HOME=/tmp";
	envp[1] = "PATH=/sbin:/bin:/usr/sbin:/usr/bin";
	envp[2] = 0;

	r = call_usermodehelper(argv[0], argv, envp);
	if (r != 0)
		printk("pm: user mode pm_helper returned %d\n", r);
}

/* 
 * pm_suggest_suspend()
 *
 * This gets called elsewhere in the kernel when the system should be placed
 * into suspend mode. For example, a power button handler.
 * It calls the user level power management handler.
 */
int pm_suggest_suspend(void)
{
	run_pm_helper(PM_SUSPEND);
	return 0;
}
#endif


#if defined (CONFIG_SYSCTL)
static int sysctl_pm_do_suspend(ctl_table *table, int write, struct file *filp,
                  void *buffer, size_t *lenp)
{
	int r;

	*lenp = 0;

	DPRINTK("System attempting to suspend.\n");
	r = pm_send_all(PM_SUSPEND, (void *)2);
	if (r) {
		return r;
	}

	r = cpu_suspend();

	pm_send_all(PM_RESUME, (void *)0);
#if defined (CONFIG_PM_HELPER)
	run_pm_helper(PM_RESUME);
#endif

	DPRINTK("System resumed normal operation.\n");
	return r;
}


static struct ctl_table pm_table[] = 
{
	{ACPI_S1_SLP_TYP, "suspend", NULL, 0, 0600, NULL, (proc_handler *)&sysctl_pm_do_suspend},
#if defined (CONFIG_PM_HELPER)
	{2, "pm_helper", pm_helper_path, sizeof(pm_helper_path), 0644, NULL, (proc_handler *)&proc_dostring},
#endif
	{0}
};

static struct ctl_table pm_dir_table[] =
{
	{CTL_ACPI, "pm", NULL, 0, 0555, pm_table},
	{0}
};


/*
 * Initialize power interface
 */
static int __init pm_init(void)
{
	printk("Power management driver for 68328, Daniel Potts <danielp@cse.unsw.edu.au>\n");

	register_sysctl_table(pm_dir_table, 1);
	return 0;
}

__initcall(pm_init);

#endif /* CONFIG_SYSCTL */


#if defined (CONFIG_PM_POWER_BUTTON_IRQ3)
/* EXAMPLE power button handler:
 *  Here we use IRQ3 as our wake up from sleep mode interrupt
 */
static void power_button_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long flags;

	save_flags(flags);
	cli();
	
	ISR |= ISR_IRQ3; /* ack edge interrupt */

	restore_flags(flags);
}

static int __init power_button(void)
{
	int err;
	
	/* init interrupt section */
	PDDIR &= ~PD_IRQ3;
	PDSEL &= ~PD_IRQ3;
	
	ICR |= ICR_ET3;  /* edge sensitive */
	ISR |= ISR_IRQ3; /* clear it */
	
	set_sleep_mask(IMR_MIRQ3);
	
	err = request_irq(IRQ3_IRQ_NUM, power_button_irq,
			  IRQ_FLG_STD, "power button", NULL);
	
	if(err)
		printk("power irq failed\n");
	return 0;
}

__initcall(power_button);

#endif /* CONFIG_PM_POWER_BUTTON_IRQ3 */
