#include <linux/config.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/mm.h>

#ifdef CONFIG_USB_OHCI

/*
 * The SA-1111 errata says that the DMA hardware needs to be exercised
 * before the clocks are turned on to work properly.  This code does
 * a tiny dma transfer to prime to hardware.
 *
 *  What DMA errata?  I've checked October 1999 and February 2001, both
 *  of which do not mention such a bug, let alone any details of this
 *  work-around.
 */
static void __init sa1111_dma_setup(void)
{
	dma_addr_t dma_buf;
	void * vbuf;

	/* DMA init & setup */

	/* WARNING: The SA-1111 L3 function is used as part of this
	 * SA-1111 DMA errata workaround.
	 *
	 * N.B., When the L3 function is enabled, it uses GPIO_B<4:5>
	 * and takes precedence over the PS/2 mouse and GPIO_B
	 * functions. Refer to "Intel StrongARM SA-1111 Microprocessor
	 * Companion Chip, Sect 10.2" for details.  So this "fix" may
	 * "break" support of either PS/2 mouse or GPIO_B if
	 * precautions are not taken to avoid collisions in
	 * configuration and use of these pins. AFAIK, no precautions
	 * are taken at this time. So it is likely that the action
	 * taken here may cause problems in PS/2 mouse and/or GPIO_B
	 * pin use elsewhere.
	 *
	 * But wait, there's more... What we're doing here is
	 * obviously altogether a bad idea. We're indiscrimanately bit
	 * flipping config for a few different functions here which
	 * are "owned" by other drivers. This needs to be handled
	 * better than it is being done here at this time.  */

	/* prime the dma engine with a tiny dma */
	SKPCR |= SKPCR_I2SCLKEN;
	SKAUD |= SKPCR_L3CLKEN | SKPCR_SCLKEN;

	SACR0 |= 0x00003305;
	SACR1 = 0x00000000;

	/*
	 * We need memory below 1MB.
	 *  NOTE: consistent_alloc gives you some random virtual
	 *  address as its return value, and the DMA address via
	 *  the dma_addr_t pointer.
	 */
	vbuf = consistent_alloc(GFP_KERNEL | GFP_DMA, 4, &dma_buf);

	SADTSA = (unsigned long)dma_buf;
	SADTCA = 4;

	SADTCS |= 0x00000011;
	SKPCR |= SKPCR_DCLKEN;

	/* wait */
	udelay(100);

	/* clear reserved but, then disable SAC */
	SACR0 &= ~(0x00000002);
	SACR0 &= ~(0x00000001);

	/* toggle bit clock direction */
	SACR0 |= 0x00000004;
	SACR0 &= ~(0x00000004);

	SKAUD &= ~(SKPCR_L3CLKEN | SKPCR_SCLKEN);

	SKPCR &= ~SKPCR_I2SCLKEN;

	consistent_free(vbuf, 4, dma_buf);
}

/*
 * reset the SA-1111 usb controller and turn on it's clocks
 */
int __init sa1111_ohci_hcd_init(void)
{
	unsigned int usb_reset = 0;

	if (machine_is_xp860() ||
	    machine_has_neponset() ||
	    machine_is_pfs168() ||
	    machine_is_badge4())
		usb_reset = USB_RESET_PWRSENSELOW | USB_RESET_PWRCTRLLOW;

	/*
	 * turn on USB clocks
	 */
	SKPCR |= SKPCR_UCLKEN;
	udelay(100);

	/*
	 * Force USB reset
	 */
	USB_RESET = USB_RESET_FORCEIFRESET;
	USB_RESET |= USB_RESET_FORCEHCRESET;
	udelay(100);

	/*
	 * Take out of reset
	 */
	USB_RESET = 0;

	/*
	 * set power sense and control lines (this from the diags code)
	 */
        USB_RESET = usb_reset;

        /*
         * Huh?  This is a _read only_ register --rmk
         */
	USB_STATUS = 0;

	udelay(10);

	/*
	 * compensate for dma bug
	 */
	sa1111_dma_setup();

	return 0;
}

void sa1111_ohci_hcd_cleanup(void)
{
	/* turn the USB clock off */
	SKPCR &= ~SKPCR_UCLKEN;
}

#endif /* CONFIG_USB_OHCI */
