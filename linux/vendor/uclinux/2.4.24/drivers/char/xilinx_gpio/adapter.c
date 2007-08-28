/*
 * adapter.c
 *
 * Xilinx GPIO Adapter component to interface GPIO component to Linux
 *
 * Author: MontaVista Software, Inc.
 *         source@mvista.com
 *
 * 2002 (c) MontaVista, Software, Inc.  This file is licensed under the terms
 * of the GNU General Public License version 2.  This program is licensed
 * "as is" without any warranty of any kind, whether express or implied.
 */

/*
 * This driver is a bit unusual in that it is composed of two logical
 * parts where one part is the OS independent code and the other part is
 * the OS dependent code.  Xilinx provides their drivers split in this
 * fashion.  This file represents the Linux OS dependent part known as
 * the Linux adapter.  The other files in this directory are the OS
 * independent files as provided by Xilinx with no changes made to them.
 * The names exported by those files begin with XGpio_.  All functions
 * in this file that are called by Linux have names that begin with
 * xgpio_.  Any other functions are static helper functions.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

/* Use the IBM OCP definitions for compatibility. */
#include <linux/ibm_ocp_gpio.h> 

#include "xbasic_types.h"
#include <asm/xparameters.h>
#include "xgpio.h"
#include "xgpio_i.h"

MODULE_AUTHOR("MontaVista Software, Inc. <source@mvista.com>");
MODULE_DESCRIPTION("Xilinx GPIO driver");
MODULE_LICENSE("GPL");

/* Our private per interface data. */
struct xgpio_instance {
	struct xgpio_instance *next_inst;	/* The next instance in inst_list */
	int index;		/* Which interface is this */
	u32 save_BaseAddress;	/* Saved physical base address */
	/*
	 * The underlying OS independent code needs space as well.  A
	 * pointer to the following XGpio structure will be passed to
	 * any XGpio_ function that requires it.  However, we treat the
	 * data as an opaque object in this file (meaning that we never
	 * reference any of the fields inside of the structure).
	 */
	XGpio Gpio;
};
/* List of instances we're handling. */
static struct xgpio_instance *inst_list = NULL;

/* SAATODO: This function will be moved into the Xilinx code. */
/*****************************************************************************/
/**
*
* Lookup the device configuration based on the GPIO instance.  The table
* XGpio_ConfigTable contains the configuration info for each device in the system.
*
* @param Instance is the index of the interface being looked up.
*
* @return
*
* A pointer to the configuration table entry corresponding to the given
* device ID, or NULL if no match is found.
*
* @note
*
* None.
*
******************************************************************************/
XGpio_Config *XGpio_GetConfig(int Instance)
{
	if (Instance < 0 || Instance >= XPAR_XGPIO_NUM_INSTANCES)
	{
		return NULL;
	}

	return &XGpio_ConfigTable[Instance];
}

/* SAATODO: This function will be moved into the Xilinx code. */
/****************************************************************************/
/**
* Get the input/output direction of all discrete signals.
*
* @param InstancePtr is a pointer to an XGpio instance to be worked on.
*
* @return Current copy of the tristate (direction) register.
*
* @note
*
* None
*
*****************************************************************************/
u32
XGpio_GetDataDirection(XGpio * InstancePtr)
{
	XASSERT_NONVOID(InstancePtr != NULL);
	XASSERT_NONVOID(InstancePtr->IsReady == XCOMPONENT_IS_READY);
	return XGpio_mReadReg(InstancePtr->BaseAddress, XGPIO_TRI_OFFSET);
}

static int
xgpio_open(struct inode *inode, struct file *file)
{
	MOD_INC_USE_COUNT;

	return 0;
}

static int
xgpio_release(struct inode *inode, struct file *file)
{
	MOD_DEC_USE_COUNT;

	return 0;
}

static int
ioctl_setup(unsigned long arg,
	    struct ibm_gpio_ioctl_data *ioctl_data,
	    struct xgpio_instance **match)
{
	struct xgpio_instance *inst;

	if (copy_from_user(ioctl_data, (void *) arg, sizeof (*ioctl_data)))
		return -EFAULT;

	inst = inst_list;
	while (inst && inst->index != ioctl_data->device)
		inst = inst->next_inst;

	*match = inst;
	return inst ? 0 : -ENODEV;
}

static int
xgpio_ioctl(struct inode *inode, struct file *file,
	    unsigned int cmd, unsigned long arg)
{
	struct ibm_gpio_ioctl_data ioctl_data;
	struct xgpio_instance *inst;
	int status;
	u32 r;

	switch (cmd) {
	case IBMGPIO_IN:
		status = ioctl_setup(arg, &ioctl_data, &inst);
		if (status < 0)
			return status;

#if 0
		/* Ensure that the GPIO bits in the mask are tristated. */
		r = XGpio_GetDataDirection(&inst->Gpio);
		XGpio_SetDataDirection(&inst->Gpio, r & ioctl_data.mask);
#endif

		ioctl_data.data = (XGpio_DiscreteRead(&inst->Gpio));

		if (copy_to_user((struct ibm_gpio_ioctl_data *) arg,
				 &ioctl_data, sizeof (ioctl_data))) {
			return -EFAULT;
		}
		break;

	case IBMGPIO_OUT:
		status = ioctl_setup(arg, &ioctl_data, &inst);
		if (status < 0)
			return status;

#if 0
		/* Get the prior value. */
		r = XGpio_DiscreteRead(&inst->Gpio);
		/* Clear the bits that we're going to put in. */
		r &= ~ioctl_data.mask;
		/* Set the bits that were provided. */
		r |= (ioctl_data.mask & ioctl_data.data);
#else
		r=ioctl_data.data;
#endif

		XGpio_DiscreteWrite(&inst->Gpio, r);

#if 0
		/* Ensure that the GPIO bits in the mask are not tristated. */
		r = XGpio_GetDataDirection(&inst->Gpio);
		XGpio_SetDataDirection(&inst->Gpio, r | ioctl_data.mask);
#endif

		break;

	case IBMGPIO_TRISTATE:
		status = ioctl_setup(arg, &ioctl_data, &inst);
		if (status < 0)
			return status;

#if 0
		/* Get the prior value. */
		r = XGpio_GetDataDirection(&inst->Gpio);
		/* Clear the bits that we're going to put in. */
		r &= ~ioctl_data.mask;
		/* Set the bits that were provided. */
		r |= (ioctl_data.mask & ioctl_data.data);
#else
		r = ioctl_data.mask;

#endif
		XGpio_SetDataDirection(&inst->Gpio, r);
		break;

	default:
		return -ENOIOCTLCMD;

	}
	return 0;
}

static void
remove_head_inst(void)
{
	struct xgpio_instance *inst;
	XGpio_Config *cfg;

	/* Pull the head off of inst_list. */
	inst = inst_list;
	inst_list = inst->next_inst;

	cfg = XGpio_GetConfig(inst->index);
	iounmap((void *) cfg->BaseAddress);
	cfg->BaseAddress = inst->save_BaseAddress;
}

static struct file_operations xfops = {
	owner:THIS_MODULE,
	ioctl:xgpio_ioctl,
	open:xgpio_open,
	release:xgpio_release,
};
/*
 * We get to all of the GPIOs through one minor number.  Here's the
 * miscdevice that gets registered for that minor number.
 */
static struct miscdevice miscdev = {
	minor:GPIO_MINOR,
	name:"xgpio",
	fops:&xfops
};

static int __init
probe(int index)
{
	static const unsigned long remap_size
	    = XPAR_GPIO_0_HIGHADDR - XPAR_GPIO_0_BASEADDR + 1;
	struct xgpio_instance *inst;
	XGpio_Config *cfg;

	/* Find the config for our instance. */
	cfg = XGpio_GetConfig(index);
	if (!cfg)
		return -ENODEV;

	/* Allocate the inst and zero it out. */
	inst = (struct xgpio_instance *) kmalloc(sizeof (struct xgpio_instance),
						 GFP_KERNEL);
	if (!inst) {
		printk(KERN_ERR "%s #%d: Could not allocate instance.\n",
		       miscdev.name, index);
		return -ENOMEM;
	}
	memset(inst, 0, sizeof (struct xgpio_instance));
	inst->index = index;

	/* Make it the head of inst_list. */
	inst->next_inst = inst_list;
	inst_list = inst;

	/* Change the addresses to be virtual; save the old ones to restore. */
	inst->save_BaseAddress = cfg->BaseAddress;
	cfg->BaseAddress = (u32) ioremap(inst->save_BaseAddress, remap_size);

	/* Tell the Xilinx code to bring this GPIO interface up. */
	if (XGpio_Initialize(&inst->Gpio, cfg->DeviceId) != XST_SUCCESS) {
		printk(KERN_ERR "%s #%d: Could not initialize instance.\n",
		       miscdev.name, inst->index);
		remove_head_inst();
		return -ENODEV;
	}

	printk(KERN_INFO "%s #%d at 0x%08X mapped to 0x%08X\n",
	       miscdev.name, inst->index,
	       inst->save_BaseAddress, cfg->BaseAddress);

	return 0;
}

static int __init
xgpio_init(void)
{
	int rtn, index = 0;

	while (probe(index++) == 0) ;

	if (index > 1) {
		/* We found at least one instance. */

		/* Register the driver with misc and report success. */
		rtn = misc_register(&miscdev);
		if (rtn) {
			printk(KERN_ERR "%s: Could not register driver.\n",
			       miscdev.name);
			while (inst_list)
				remove_head_inst();
			return rtn;
		}

		/* Report success. */
		printk("Xilinx GPIO registered\n");
		return 0;
	} else {
		/* No instances found. */
		return -ENODEV;
	}

}

static void __exit
xgpio_cleanup(void)
{
	while (inst_list)
		remove_head_inst();

	misc_deregister(&miscdev);
}

EXPORT_NO_SYMBOLS;

module_init(xgpio_init);
module_exit(xgpio_cleanup);
