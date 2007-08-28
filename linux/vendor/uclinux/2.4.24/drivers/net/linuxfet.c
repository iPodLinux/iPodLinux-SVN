
/*
**
**
**  VIA Technologies, Inc.              
**
**  Fast Ethernet Adapter
**
**  Linux Driver
**
**  v3.28  Nov. 2001
**
**
*/

/* These identify the driver base version and may not be removed. */
static const char version[] = "linuxfet.c : v3.28 11/15/2001\n";

#include "linuxfet.h"
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
static void *via_probe1(struct pci_dev *pdev, void *init_dev, unsigned long ioaddr, int irq, int chip_idx, int find_cnt, u32 revision);
static int  pci_drv_register(drv_id_info *drv_id, void *initial_device);
static void pci_drv_unregister(drv_id_info *drv_id);
static void VLAN_tagging(struct net_device *dev);
static void turn_on_MII_link(struct net_device *dev);
static int  pci_find(struct pci_dev *pdev, int findtype);
static int  acpi_wake(struct pci_dev *pdev);
static int init_device_data(struct net_device *dev, struct pci_dev *pdev, unsigned long ioaddr, u32 revision, int irq, int chip_idx, int card_idx);
static void SafeDisableMiiAuto(struct net_device *dev);
static unsigned int  mdio_read(struct net_device *dev, int phy_id, int location);
static void mdio_write(struct net_device *dev, int phy_id, int location, unsigned int value);
//static void CAM_data_read(struct net_device *dev, int select_CAM, unsigned char CAM_address, unsigned char *value);
static void CAM_data_write(struct net_device *dev, int select_CAM, unsigned char CAM_address, unsigned char *value);
//static unsigned int CAM_mask_read(struct net_device *dev, int select_CAM);
static void CAM_mask_write(struct net_device *dev, int select_CAM, unsigned int mask);
//static void netdev_reset(struct net_device *dev);
static int  netdev_open(struct net_device *dev);
static void set_flow_control(struct net_device *dev);
static void flow_control_ability(struct net_device *dev);
static void set_media_duplex_mode(struct net_device *dev);
static void restart_autonegotiation(struct net_device *dev);
static void enable_autonegotiation(struct net_device *dev);
static void check_legacy_force(struct net_device *dev);
static int check_n_way_force(struct net_device *dev, int change_flag);
static void do_autonegotiation(struct net_device *dev);
//static void netdev_timer(unsigned long data);
//static void tx_timeout(struct net_device *dev);
static void init_ring(struct net_device *dev);
static int  start_tx(struct sk_buff *skb, struct net_device *dev);
static void intr_handler(int irq, void *dev_instance, struct pt_regs *regs);
static int  netdev_rx(struct net_device *dev);
static void  netdev_tx(struct net_device *dev);
static void netdev_error(struct net_device *dev, int intr_status);
static void set_rx_mode(struct net_device *dev);
static struct net_device_stats *get_stats(struct net_device *dev);
static int  mii_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
static int  netdev_close(struct net_device *dev);
static void checksum_offload(struct sk_buff *skb, u32 rx_PQSTS);
int (*register_cb_hook)(drv_id_info *did);
void (*unregister_cb_hook)(drv_id_info *did);

static int	debug = 3;			/* 1 normal messages, 0 quiet .. 7 verbose. */


static int	min_pci_latency = 32;


/* The user-configurable values.
   These may be modified when a driver module is loaded.*/   
static int	max_interrupt_work = 20;

/* Set the copy breakpoint for the copy-only-tiny-frames scheme.
   Setting to > 1518 effectively disables this feature. */
static int	rx_copybreak = 0;

#define	MAX_UNITS 8     /* More are supported, limit only on speed_duplex */

/* speed_duplex[] is used for setting the speed and duplex mode of NIC.
   0: indicate autonegotiation for both speed and duplex mode
   1: indicate 100Mbps half duplex mode
   2: indicate 100Mbps full duplex mode
   3: indicate 10Mbps half duplex mode
   4: indicate 10Mbps full duplex mode
*/
static int      speed_duplex[MAX_UNITS]= {0, 0, 0, 0, 0, 0, 0, 0};

/* enable_tagging[] is used for enabling VID setting.
   0: disable VID seeting(default).
   1: enable VID setting.
*/
static int      enable_tagging[MAX_UNITS]={0, 0, 0, 0, 0, 0, 0, 0};

/* VID_setting[] is used for setting the VID of NIC.
   0: default VID.
   1-4094: other VIDs.
*/
static int      VID_setting[MAX_UNITS]={0, 0, 0, 0, 0, 0, 0, 0};

/* csum_offload[] is used for setting the checksum offload ability of NIC.
   0: disable csum_offload[checksum offload(default).
   1: enable checksum offload. (We only support RX checksum offload now)
*/
static int      csum_offload[MAX_UNITS]={0, 0, 0, 0, 0, 0, 0, 0};

/* flow_control[] is used for setting the flow control ability of NIC.
   1: hardware deafult(default). Use Hardware default value in ANAR.
   2: disable PAUSE in ANAR.
   3: enable PAUSE in ANAR.
*/
static int      flow_control[MAX_UNITS]={1, 1, 1, 1, 1, 1, 1, 1};

/* IP_byte_align[] is used for IP header DWORD byte aligned
   0: indicate the IP header won't be DWORD byte aligned.(Default) .
   1: indicate the IP header will be DWORD byte aligned.
      In some enviroment, the IP header should be DWORD byte aligned,
      or the packet will be droped when we receive it. (eg: IPVS)
*/
static int	IP_byte_align[MAX_UNITS] = {0, 0, 0, 0, 0, 0, 0, 0};

/* tx_thresh[] is used for controlling the transmit fifo threshold.
   0: indicate the txfifo threshold is 128 bytes.
   1: indicate the txfifo threshold is 256 bytes.
   2: indicate the txfifo threshold is 512 bytes.
   3: indicate the txfifo threshold is 1024 bytes.
   4: indicate that we use store and forward
*/
static int	tx_thresh[MAX_UNITS] = {0, 0, 0, 0, 0, 0, 0, 0};

/* rx_thresh[] is used for controlling the receive fifo threshold.
   0: indicate the rxfifo threshold is 64 bytes.
   1: indicate the rxfifo threshold is 32 bytes.
   2: indicate the rxfifo threshold is 128 bytes.
   3: indicate the rxfifo threshold is 256 bytes.
   4: indicate the rxfifo threshold is 512 bytes.
   5: indicate the rxfifo threshold is 768 bytes.
   6: indicate the rxfifo threshold is 1024 bytes.   
   7: indicate that we use store and forward
*/
static int	rx_thresh[MAX_UNITS] = {0, 0, 0, 0, 0, 0, 0, 0};

/* DMA_length[] is used for controlling the DMA length
   0: 8 DWORDs
   1: 16 DWORDs
   2: 32 DWORDs
   3: 64 DWORDs
   4: 128 DWORDs
   5: 256 DWORDs
   6: SF(flush till emply)
   7: SF(flush till emply)
*/
static int	DMA_length[MAX_UNITS] = {1, 1, 1, 1, 1, 1, 1, 1};

/* Maximum number of multicast addresses to filter (vs. rx-all-multicast).
   The Rhine has a 64 element 8390-like hash table.  */
static const int	multicast_filter_limit = 32;

pci_id_info pci_tbl[] = {
    {"VIA VT86C100A Fast Ethernet Adapter                         ", 
        {0x30431106, 0xffffffff, 0x01001106, 0xffffffff, },
        RHINE_IOTYPE, 128, FET_CanHaveMII | FET_ReqTxAlign | FET_HasDavicomPhy},
    {"VIA PCI 10/100Mb Fast Ethernet Adapter                      ",
        {0x30651106, 0xffffffff, 0x01021106, 0xffffffff, },
        RHINE_IOTYPE, RHINEII_IOSIZE, FET_CanHaveMII | FET_HasWOL },
    {"VIA VT6105 Rhine III Management Adapter                     ",
        {0x31061106, 0xffffffff, 0x01051106, 0xffffffff, },
        RHINE_IOTYPE, RHINEII_IOSIZE, FET_CanHaveMII | FET_HasWOL },
    {0,},						/* 0 terminated list. */
};


drv_id_info via_rhine_drv_id = {
    "linuxfet", 0, PCI_CLASS_NETWORK_ETHERNET<<8, pci_tbl, via_probe1,};
/* Offsets to the device registers.
*/


#if LINUX_VERSION_CODE > 0x20118  &&  defined(MODULE)
MODULE_AUTHOR("Donald Becker <becker@scyld.com>");
MODULE_DESCRIPTION("PCI 10/100Mb Fast Ethernet Adapter");
MODULE_PARM(debug, "i");
MODULE_PARM(min_pci_latency, "i");
MODULE_PARM(max_interrupt_work, "i");
MODULE_PARM(rx_copybreak, "i");
MODULE_PARM(speed_duplex, "1-" __MODULE_STRING(MAX_UNITS) "i");
MODULE_PARM(VID_setting, "1-" __MODULE_STRING(MAX_UNITS) "i");
MODULE_PARM(csum_offload, "1-" __MODULE_STRING(MAX_UNITS) "i");
MODULE_PARM(enable_tagging, "1-" __MODULE_STRING(MAX_UNITS) "i");
MODULE_PARM(flow_control, "1-" __MODULE_STRING(MAX_UNITS) "i");
MODULE_PARM(IP_byte_align, "1-" __MODULE_STRING(MAX_UNITS) "i");
MODULE_PARM(tx_thresh, "1-" __MODULE_STRING(MAX_UNITS) "i");
MODULE_PARM(rx_thresh, "1-" __MODULE_STRING(MAX_UNITS) "i");
MODULE_PARM(DMA_length, "1-" __MODULE_STRING(MAX_UNITS) "i");

#endif


/* A list of our installed devices, for removing the driver module. */
static struct net_device *root_net_dev = NULL;

#if (LINUX_VERSION_CODE >= 0x20200)
/* Grrrr.. complex abstaction layers with negative benefit. */
static int pci_drv_register(drv_id_info *drv_id, void *initial_device)
{
	int chip_idx, cards_found = 0;
	struct pci_dev *pdev = NULL;
	pci_id_info *pci_tbl = drv_id->pci_dev_tbl;
	void *newdev;
        u32 revision;

	while ((pdev = pci_find_class(drv_id->pci_class, pdev)) != 0) {
		u32 pci_id, pci_subsys_id, pci_class_rev;
		u16 pci_command, new_command;
		int pci_flags;
		long pciaddr;			/* Bus address. */
		unsigned long ioaddr;			/* Mapped address for this processor. */
                
		pci_read_config_dword(pdev, PCI_VENDOR_ID, &pci_id);
		/* Offset 0x2c is PCI_SUBSYSTEM_ID aka PCI_SUBSYSTEM_VENDOR_ID. */
		pci_read_config_dword(pdev, 0x2c, &pci_subsys_id);
		pci_read_config_dword(pdev, PCI_REVISION_ID, &pci_class_rev);
                
                revision = (pci_class_rev << 24) >> 24;
		if (debug > 3)
			printk(KERN_DEBUG "PCI ID %8.8x subsystem ID is %8.8x.\n",
				   pci_id, pci_subsys_id);
		for (chip_idx = 0; pci_tbl[chip_idx].name; chip_idx++) {
			pci_id_info *chip = &pci_tbl[chip_idx];
			if ((pci_id & chip->id.pci_mask) == chip->id.pci
				&& (pci_subsys_id&chip->id.subsystem_mask) == chip->id.subsystem
				&& (pci_class_rev&chip->id.revision_mask) == chip->id.revision)
				break;
		}
		if (pci_tbl[chip_idx].name == 0) 		/* Compiled out! */
			continue;

		pci_flags = pci_tbl[chip_idx].pci_flags;
#if (LINUX_VERSION_CODE >= 0x02030c)
		/* Wow. A oversized, hard-to-use abstraction. Bogus. */
		pciaddr = pdev->resource[(pci_flags >> 4) & 7].start;
                ioaddr = pci_resource_start(pdev, 0);

#else /* LINUX_VERSION_CODE >= 0x20200 */
		pciaddr = pdev->base_address[(pci_flags >> 4) & 7];
#if defined(__alpha__)			/* Really any machine with 64 bit addressing. */
		if (pci_flags & FET_PCI_ADDR_64BITS)
			pciaddr |= ((long)pdev->base_address[((pci_flags>>4)&7)+ 1]) << 32;
#endif /* defined(__alpha__) */

		if (debug > 3)
			printk(KERN_INFO "Found %s at PCI address %#lx, mapped IRQ %d.\n",
				   pci_tbl[chip_idx].name, pciaddr, pdev->irq);

		if ((pciaddr & PCI_BASE_ADDRESS_SPACE_IO)) {
			ioaddr = pciaddr & PCI_BASE_ADDRESS_IO_MASK;
			if (check_region(ioaddr, pci_tbl[chip_idx].io_size))
				continue;
		} else if ((ioaddr = (long)ioremap(pciaddr & PCI_BASE_ADDRESS_MEM_MASK,
										   pci_tbl[chip_idx].io_size)) == 0) {
			printk(KERN_INFO "Failed to map PCI address %#lx for device "
				   "'%s'.\n", pciaddr, pci_tbl[chip_idx].name);
			continue;
		}
#endif /* LINUX_VERSION_CODE >= 0x20200 */
		if ( ! (pci_flags & FET_PCI_NO_ACPI_WAKE))
			acpi_wake(pdev);
		pci_read_config_word(pdev, PCI_COMMAND, &pci_command);
		new_command = pci_command | (pci_flags & 7);
		if (pci_command != new_command) {
			printk(KERN_INFO "  The PCI BIOS has not enabled the"
				   " device at %d/%d!  Updating PCI command %4.4x->%4.4x.\n",
				   pdev->bus->number, pdev->devfn, pci_command, new_command);
			pci_write_config_word(pdev, PCI_COMMAND, new_command);
		}

		newdev = drv_id->probe1(pdev, initial_device,
								ioaddr, pdev->irq, chip_idx, cards_found, revision);
		if (newdev  && (pci_flags & PCI_COMMAND_MASTER))
			pci_set_master(pdev);
		if (newdev  && (pci_flags & PCI_COMMAND_MASTER)  &&
			! (pci_flags & FET_PCI_NO_MIN_LATENCY)) {
			u8 pci_latency;
			pci_read_config_byte(pdev, PCI_LATENCY_TIMER, &pci_latency);
			if (pci_latency < min_pci_latency) {
				printk(KERN_INFO "  PCI latency timer (CFLT) is "
					   "unreasonably low at %d.  Setting to %d clocks.\n",
					   pci_latency, min_pci_latency);
				pci_write_config_byte(pdev, PCI_LATENCY_TIMER, min_pci_latency);
			}
		}
		initial_device = 0;
		cards_found++;
	}

	if ((drv_id->flags & FET_PCI_HOTSWAP)
		&& register_cb_hook
		&& (*register_cb_hook)(drv_id) == 0) {
		MOD_INC_USE_COUNT;
		return 0;
	} else
		return cards_found ? 0 : -ENODEV;
}
#else /* LINUX_VERSION_CODE >= 0x20200 */
static int pci_drv_register(drv_id_info *drv_id, void *initial_device)
{
	int pci_index, cards_found = 0;
	unsigned char pci_bus, pci_device_fn;
	struct pci_dev *pdev;
	pci_id_info *pci_tbl = drv_id->pci_dev_tbl;
	void *newdev;
        u32 revision;

	if ( ! pcibios_present())
		return -ENODEV;

	for (pci_index = 0; pci_index < 0xff; pci_index++) {
		u32 pci_id, subsys_id, pci_class_rev;
		u16 pci_command, new_command;
		int chip_idx, irq, pci_flags;
		long pciaddr;
		unsigned long ioaddr;
		u32 pci_busaddr;
		u8 pci_irq_line;

		if (pcibios_find_class (drv_id->pci_class, pci_index,
								&pci_bus, &pci_device_fn)
			!= PCIBIOS_SUCCESSFUL)
			break;
		pcibios_read_config_dword(pci_bus, pci_device_fn,
								  PCI_VENDOR_ID, &pci_id);
		/* Offset 0x2c is PCI_SUBSYSTEM_ID aka PCI_SUBSYSTEM_VENDOR_ID. */
		pcibios_read_config_dword(pci_bus, pci_device_fn, 0x2c, &subsys_id);
		pcibios_read_config_dword(pci_bus, pci_device_fn, PCI_REVISION_ID, &pci_class_rev);

		for (chip_idx = 0; pci_tbl[chip_idx].name; chip_idx++) {
			pci_id_info *chip = &pci_tbl[chip_idx];
			if ((pci_id & chip->id.pci_mask) == chip->id.pci
				&& (subsys_id & chip->id.subsystem_mask) == chip->id.subsystem
				&& (pci_class_rev&chip->id.revision_mask) == chip->id.revision)
				break;
		}
		if (pci_tbl[chip_idx].name == 0) 		/* Compiled out! */
			continue;

		pci_flags = pci_tbl[chip_idx].pci_flags;
		pdev = pci_find_slot(pci_bus, pci_device_fn);
		pcibios_read_config_byte(pci_bus, pci_device_fn,
								 PCI_INTERRUPT_LINE, &pci_irq_line);
		irq = pci_irq_line;
		pcibios_read_config_dword(pci_bus, pci_device_fn,
								  ((pci_flags >> 2) & 0x1C) + 0x10,
								  &pci_busaddr);
		pciaddr = pci_busaddr;
#if defined(__alpha__)
		if (pci_flags & FET_PCI_ADDR_64BITS) {
			pcibios_read_config_dword(pci_bus, pci_device_fn,
									  ((pci_flags >> 2) & 0x1C) + 0x14,
									  &pci_busaddr);
			pciaddr |= ((long)pci_busaddr)<<32;
		}
#endif /* defined(__alpha__) */

		if (debug > 3)
			printk(KERN_INFO "Found %s at PCI address %#lx, IRQ %d.\n",
				   pci_tbl[chip_idx].name, pciaddr, irq);

		if ((pciaddr & PCI_BASE_ADDRESS_SPACE_IO)) {
			ioaddr = pciaddr & PCI_BASE_ADDRESS_IO_MASK;
			if (check_region(ioaddr, pci_tbl[chip_idx].io_size))
				continue;
		} else if ((ioaddr = (long)ioremap(pciaddr & PCI_BASE_ADDRESS_MEM_MASK,
										   pci_tbl[chip_idx].io_size)) == 0) {
			printk(KERN_INFO "Failed to map PCI address %#lx.\n",
				   pciaddr);
			continue;
		}

		if ( ! (pci_flags & FET_PCI_NO_ACPI_WAKE))
			acpi_wake(pdev);
		pcibios_read_config_word(pci_bus, pci_device_fn,
								 PCI_COMMAND, &pci_command);
		new_command = pci_command | (pci_flags & 7);
		if (pci_command != new_command) {
			printk(KERN_INFO "  The PCI BIOS has not enabled the"
				   " device at %d/%d!  Updating PCI command %4.4x->%4.4x.\n",
				   pci_bus, pci_device_fn, pci_command, new_command);
			pcibios_write_config_word(pci_bus, pci_device_fn,
									  PCI_COMMAND, new_command);
		}

		newdev = drv_id->probe1(pdev, initial_device,
							   ioaddr, irq, chip_idx, cards_found, revision);

		if (newdev  && (pci_flags & PCI_COMMAND_MASTER)  &&
			! (pci_flags & FET_PCI_NO_MIN_LATENCY)) {
			u8 pci_latency;
			pcibios_read_config_byte(pci_bus, pci_device_fn,
									 PCI_LATENCY_TIMER, &pci_latency);
			if (pci_latency < min_pci_latency) {
				printk(KERN_INFO "  PCI latency timer (CFLT) is "
					   "unreasonably low at %d.  Setting to %d clocks.\n",
					   pci_latency, min_pci_latency);
				pcibios_write_config_byte(pci_bus, pci_device_fn,
										  PCI_LATENCY_TIMER, min_pci_latency);
			}
		}
		initial_device = 0;
		cards_found++;
	}

	if ((drv_id->flags & FET_PCI_HOTSWAP)
		&& register_cb_hook
		&& (*register_cb_hook)(drv_id) == 0) {
		MOD_INC_USE_COUNT;
		return 0;
	} else
		return cards_found ? 0 : -ENODEV;
}
#endif /* LINUX_VERSION_CODE >= 0x20200 */


static void pci_drv_unregister(drv_id_info *drv_id)
{
	/* We need do something only with CardBus support. */
	if (unregister_cb_hook) {
		(*unregister_cb_hook)(drv_id);
		MOD_DEC_USE_COUNT;
	}
	return;
}

/*
  Search PCI configuration space for the specified capability registers.
  Return the index, or 0 on failure.
*/
static int pci_find(struct pci_dev *pdev, int findtype)
{
	u16 pci_status, cap_type;
	u8 pci_cap_idx;
	int cap_idx;

	pci_read_config_word(pdev, PCI_STATUS, &pci_status);
	if ( ! (pci_status & PCI_STATUS_CAP_LIST))
		return 0;
	pci_read_config_byte(pdev, PCI_CAPABILITY_LIST, &pci_cap_idx);
	cap_idx = pci_cap_idx;
	for (cap_idx = pci_cap_idx; cap_idx; cap_idx = (cap_type >> 8) & 0xff) {
		pci_read_config_word(pdev, cap_idx, &cap_type);
		if ((cap_type & 0xff) == findtype)
			return cap_idx;
	}
	return 0;
}


/* Change a device from D3 (sleep) to D0 (active).
   Return the old power state.
   This is more complicated than you might first expect since most cards
   forget all PCI config info during the transition! */
static int acpi_wake(struct pci_dev *pdev)
{
	u32 base[5], romaddr;
	u16 pci_command, pwr_command;
	u8  pci_latency, pci_cacheline, irq;
	int i, pwr_cmd_idx = pci_find(pdev, PCI_CAP_ID_PM);

	if (pwr_cmd_idx == 0)
		return 0;
	pci_read_config_word(pdev, pwr_cmd_idx + 4, &pwr_command);
	if ((pwr_command & 3) == 0)
		return 0;
	pci_read_config_word(pdev, PCI_COMMAND, &pci_command);
	for (i = 0; i < 5; i++)
		pci_read_config_dword(pdev, PCI_BASE_ADDRESS_0 + i*4,
								  &base[i]);
	pci_read_config_dword(pdev, PCI_ROM_ADDRESS, &romaddr);
	pci_read_config_byte( pdev, PCI_LATENCY_TIMER, &pci_latency);
	pci_read_config_byte( pdev, PCI_CACHE_LINE_SIZE, &pci_cacheline);
	pci_read_config_byte( pdev, PCI_INTERRUPT_LINE, &irq);

	pci_write_config_word(pdev, pwr_cmd_idx + 4, 0x0000);
	for (i = 0; i < 5; i++)
		if (base[i])
			pci_write_config_dword(pdev, PCI_BASE_ADDRESS_0 + i*4,
									   base[i]);
	pci_write_config_dword(pdev, PCI_ROM_ADDRESS, romaddr);
	pci_write_config_byte( pdev, PCI_INTERRUPT_LINE, irq);
	pci_write_config_byte( pdev, PCI_CACHE_LINE_SIZE, pci_cacheline);
	pci_write_config_byte( pdev, PCI_LATENCY_TIMER, pci_latency);
	pci_write_config_word( pdev, PCI_COMMAND, pci_command | 5);
	return pwr_command & 3;
}
static void *via_probe1(struct pci_dev *pdev, void *init_dev,
                        unsigned long ioaddr, int irq, int chip_idx, int card_idx, u32 revision)
{
    struct net_device *dev;
    int i;
    unsigned char byOrgValue;
    int ww;
    u8 mode3_reg;

    // if vt3065     
    if (revision>=0x40) {
       // clear sticky bit before reset & read ethernet address
       byOrgValue = readb(ioaddr + FET_STICKHW);    
       byOrgValue = byOrgValue & 0xFC;
       writeb(byOrgValue, ioaddr + FET_STICKHW);        
       // disable force PME-enable 
       writeb(0x80, ioaddr + FET_WOLCG_CLR);
       // disable power-event config bit
       writeb(0xFF, ioaddr + FET_WOLCR_CLR);
       // clear power status 
       writeb(0xFF, ioaddr + FET_PWRCSR_CLR);   
    }
    
        
    /* Reset the chip to erase previous misconfiguration. */
    writew(FET_CmdReset, ioaddr + FET_CR0);
    // if vt3043 delay after reset
    if (revision <0x40) {
       udelay(10000);
    }

    // polling till software reset complete
    // W_MAX_TIMEOUT is the timeout period
    for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
        if ((readw(ioaddr + FET_CR0) & FET_CmdReset) == 0 )
            break;
    }
       
    // issue AUTOLoad in EECSR to reload eeprom
    writeb(0x20, ioaddr + FET_EECSR);
      
    // if vt3065 delay after reset
    if (revision >=0x40 ) { 

        // delay 8ms to let MAC stable
        mdelay(8);
        // for 3065D, EEPROM reloaded will cause bit 0 in CFGA turned on.
        // it makes MAC receive magic packet automatically. So, driver turn it off.
        writeb(readb(ioaddr + FET_CFGA) & 0xFE, ioaddr + FET_CFGA);
    }

    /* turn on bit2 in PCI configuration register 0x53 , only for 3065*/
    if (revision >= 0x40) {
        pci_read_config_byte(pdev,PCI_REG_MODE3,&mode3_reg);
        pci_write_config_byte(pdev,PCI_REG_MODE3,mode3_reg|MODE3_MIION);
    }

    /* back off algorithm ,disable the right-most 4-bit off FET_CFGD*/
    writeb(readb(ioaddr + FET_CFGD) & (~(FET_CRADOM | FET_CAP | FET_MBA | FET_BAKOPT)), ioaddr + FET_CFGD);
#if (LINUX_VERSION_CODE >= 0x20341)
    dev = init_etherdev(NULL, sizeof(netdev_private));
#else
    dev = init_etherdev(init_dev, 0);
#endif
    printk(KERN_INFO "%s: %s\n",dev->name, pci_tbl[chip_idx].name);
    printk(KERN_INFO "%s: IO Address = 0x%lx, MAC Address = ",dev->name, ioaddr);
    /* Ideally we would read the EEPROM but access may be locked. */
    for (i = 0; i < 6; i++)
        dev->dev_addr[i] = readb(ioaddr + FET_PAR + i);
    for (i = 0; i < 5; i++)
            printk("%2.2x:", dev->dev_addr[i]);
    printk("%2.2x, IRQ = %d.\n", dev->dev_addr[i], irq);

    if (init_device_data (dev, pdev, ioaddr, revision, irq, chip_idx, card_idx) == -1)
       return NULL;
    return dev;
}

static int init_device_data(struct net_device *dev, struct pci_dev *pdev, unsigned long ioaddr, u32 revision, int irq, int chip_idx, int card_idx)
{
    netdev_private *np;
#if (LINUX_VERSION_CODE < 0x20341)
    void *priv_mem;
    void *pSRdTemp, *pSRdTemp_aligned;
    void *pSTdTemp, *pSTdTemp_aligned;
#endif
    int i;

    /* Make certain the descriptor lists are cache-aligned. */
#if (LINUX_VERSION_CODE >= 0x20341)
    np = dev->priv;
    if (revision >= 0x80 && enable_tagging[np->card_idx] == 1)
        np->ring = pci_alloc_consistent(pdev,(sizeof(rx_desc) * RX_RING_SIZE)+(sizeof(tx_desc) * TX_RING_SIZE * 8), &np->ring_dma);
    else
        np->ring = pci_alloc_consistent(pdev,(sizeof(rx_desc) * RX_RING_SIZE)+(sizeof(tx_desc) * TX_RING_SIZE), &np->ring_dma);

    /* Check for the very unlikely case of no memory. */
    if (np->ring == NULL)
        return -1;
    np->rx_ring = np->ring;
    if (revision >= 0x80 && enable_tagging[np->card_idx] == 1) {
        for (i = 0; i < 8; i++) {
            np->tx_ring[i] = np->ring + (sizeof(rx_desc) * RX_RING_SIZE) + (sizeof(tx_desc) * TX_RING_SIZE * i);
        }
    }
    else {
        np->tx_ring[0] = np->ring + (sizeof(rx_desc) * RX_RING_SIZE);
        for (i = 1; i < 8; i++)
            np->tx_ring[i] = NULL;
    }
    np->pdev=pdev;
#else
    priv_mem = kmalloc(sizeof(*np) + PRIV_ALIGN, GFP_KERNEL);
//    printk("chenyp:priv_mem=%x\n",priv_mem);
    pSRdTemp=kmalloc((sizeof(rx_desc) * RX_RING_SIZE) + PRIV_ALIGN, GFP_KERNEL);
//    printk("chenyp: init_device_data: pSRdTemp=%x\n",pSRdTemp);
    if (revision >= 0x80) {
        pSTdTemp=kmalloc((sizeof(tx_desc) * TX_RING_SIZE * 8) + PRIV_ALIGN, GFP_KERNEL);
        //printk("chenyp: init_device_data: pSTdTemp=%x\n",pSTdTemp);
    }
    else {
        pSTdTemp=kmalloc((sizeof(tx_desc) * TX_RING_SIZE) + PRIV_ALIGN, GFP_KERNEL);
//        printk("chenyp:pSTdTemp=%x\n",pSTdTemp);

    }
    /* Check for the very unlikely case of no memory. */
    if (priv_mem == NULL || pSRdTemp == NULL || pSTdTemp == NULL)
        return -1;

    pSRdTemp_aligned = (void *)(((long)pSRdTemp + PRIV_ALIGN) & ~PRIV_ALIGN);
    pSTdTemp_aligned = (void *)(((long)pSTdTemp + PRIV_ALIGN) & ~PRIV_ALIGN);
    memset(pSRdTemp_aligned, 0, sizeof(rx_desc) * RX_RING_SIZE);
    if (revision >= 0x80)
        memset(pSTdTemp_aligned, 0, sizeof(rx_desc) * TX_RING_SIZE * 8);
    else
        memset(pSTdTemp_aligned, 0, sizeof(rx_desc) * TX_RING_SIZE);
    dev->priv = np = (void *)(((long)priv_mem + PRIV_ALIGN) & ~PRIV_ALIGN);
    memset(np, 0, sizeof(*np));
    np->priv_addr = priv_mem;
    np->rx_ring = (rx_desc*)pSRdTemp_aligned;
    if (revision >= 0x80) {
        for (i = 0; i < 8; i++) {
            np->tx_ring[i] = (tx_desc *)(pSTdTemp_aligned + (sizeof(rx_desc) * TX_RING_SIZE * i));
        }
    }
    else {
        np->tx_ring[0] = (tx_desc *)pSTdTemp_aligned;
        for (i = 1; i < 8; i++)
            np->tx_ring[i] = NULL;
    }
    np->priv_rd = pSRdTemp;
    np->priv_td = pSTdTemp;

#endif


#ifdef USE_IO_OPS
    request_region(ioaddr, pci_tbl[chip_idx].io_size, dev->name);
#endif /* ifdef USE_IO_OPS */

    /* Reset the chip to erase previous misconfiguration. */
//    writew(FET_CmdReset, ioaddr + FET_CR0);

    dev->base_addr = ioaddr;
    dev->irq = irq;
    np->next_module = root_net_dev;
    root_net_dev = dev;
    np->pci_dev = pdev;
    np->chip_idx = chip_idx;
    np->card_idx = card_idx;
    np->drv_flags = pci_tbl[chip_idx].drv_flags;
    np->revision =revision;

    /* The chip-specific entries in the device structure. */
    dev->open = &netdev_open;
    dev->hard_start_xmit = &start_tx;
    dev->stop = &netdev_close;
    dev->get_stats = &get_stats;
    dev->set_multicast_list = &set_rx_mode;
    dev->do_ioctl = &mii_ioctl;

    if (np->drv_flags & FET_CanHaveMII) {
        np->phy_addr = readb(ioaddr + FET_MIICFG) & 0x1f;
        np->advertising = mdio_read(dev, np->phy_addr, 4);
    }
/*
    //add by chenyp,MACM
    {
    	unsigned char value[8];

    	value[0]=0x12;
    	value[1]=0x23;
    	value[2]=0x34;
    	value[3]=0x45;
    	value[4]=0x56;
    	value[5]=0x67;
    	value[6]=0x78;
    	value[7]=0x89;
    	CAM_data_write(dev,0,0,value);

    	value[0]=0xab;
    	value[1]=0xcd;
    	value[2]=0xef;
    	value[3]=0x12;
    	value[4]=0x34;
    	value[5]=0x56;
    	value[6]=0x78;
    	value[7]=0x9a;
    	CAM_data_write(dev,0,1,value);

    	value[0]=0x55;
    	value[1]=0x55;
    	value[2]=0x55;
    	value[3]=0x55;
    	value[4]=0x66;
    	value[5]=0x66;
    	value[6]=0x66;
    	value[7]=0x66;
       CAM_data_write(dev,0,2,value);

    	value[0]=0x77;
    	value[1]=0x77;
    	value[2]=0x77;
    	value[3]=0x77;
    	value[4]=0x88;
    	value[5]=0x88;
    	value[6]=0x88;
    	value[7]=0x88;
        CAM_data_write(dev,0,3,value);

    	value[0]=0x12;
    	value[1]=0x34;
    	value[2]=0x56;
    	value[3]=0x78;
    	value[4]=0x9a;
    	value[5]=0xbc;
    	value[6]=0xde;
    	value[7]=0xf0;
    	CAM_data_write(dev,1,0,value);

    	value[0]=0x23;
    	value[1]=0x34;
    	value[2]=0x45;
    	value[3]=0x56;
    	value[4]=0x67;
    	value[5]=0x78;
    	value[6]=0x89;
    	value[7]=0x9a;
    	CAM_data_write(dev,1,1,value);

    	value[0]=0x55;
    	value[1]=0x55;
    	value[2]=0x55;
    	value[3]=0x55;
    	value[4]=0x66;
    	value[5]=0x66;
    	value[6]=0x66;
    	value[7]=0x66;
       CAM_data_write(dev,1,2,value);

    	value[0]=0x77;
    	value[1]=0x77;
    	value[2]=0x77;
    	value[3]=0x77;
    	value[4]=0x88;
    	value[5]=0x88;
    	value[6]=0x88;
    	value[7]=0x88;
        CAM_data_write(dev,1,3,value);
CAM_mask_write(dev,0,0x12345678);
printk("mask1=%x\n",CAM_mask_read(dev,0) );
CAM_mask_write(dev,1,0xabcdefab);
printk("mask2=%x\n",CAM_mask_read(dev,1) );
    }
*/
    return 0;
}

void SafeDisableMiiAuto(struct net_device *dev)
{
    unsigned long ioaddr = dev->base_addr;
    netdev_private *np = (netdev_private *)dev->priv;
    int ww;

    /* before read mii data, we must turn off mauto */
    writeb(0, ioaddr + FET_MIICR);

    // for VT3043 only
    if (np->revision < 0x20) {
        // turn off MSRCEN
        // NOTE.... address of MII should be 0x01,
        //          otherwise SRCI will invoked
        writeb(0x01, ioaddr + FET_MIIADR);
        mdelay(1);

        // turn on MAUTO
        writeb(0x80, ioaddr + FET_MIICR);

        // W_MAX_TIMEOUT is the timeout period
        for (ww = 0; ww < 0x3fff; ww++) {
            if (readb(ioaddr + FET_MIIADR) & 0x20)
                break;
        }

        // as soon as MDONE is on, 
        // this is the right time to turn off MAUTO
        writeb(0, ioaddr + FET_MIICR);
    }
    else {                   
        // as soon as MIDLE is on, MAUTO is really stoped
        for (ww = 0; ww < 0x3fff; ww++) {
            if (readb(ioaddr + FET_MIIADR) & 0x80)
                break;
        }
    }
}
/* Read and write over the MII Management Data I/O (MDIO) interface. */
static unsigned int mdio_read(struct net_device *dev, int phy_id, int regnum)
{
    unsigned long ioaddr = dev->base_addr;
    unsigned char byMIICRbak;
    unsigned int wMII_DATA_REG;
    netdev_private *np = (netdev_private *)dev->priv;
    int ww;

    /* backup MIICR*/
    byMIICRbak = readb(ioaddr + FET_MIICR);

    /* before read mii data, we must turn off MAUTO */
    SafeDisableMiiAuto(dev);

    /* write PHY id */
    writeb(phy_id, ioaddr + FET_MIICFG);

    /* write MII address */
    writeb(regnum, ioaddr + FET_MIIADR);

    /* Trigger reading */
    writeb(readb(ioaddr+FET_MIICR) | 0x40, ioaddr + FET_MIICR);   
    
    /* waiting read complete. */
    for (ww = 0; ww < 0x3fff; ww++)
        if (!(readb(ioaddr + FET_MIICR) & 0x40))
            break;

    /* read MII register's data */
    wMII_DATA_REG = readw(ioaddr + FET_MII_DATA_REG);

    /* for VT3043 only */
    if (np->revision < 0x20)
        mdelay(1);

    /* the value of MAD4-MAD0 bit must be 00001 before we turn on MAUTO */
    /* or the LNKFL bit in MIISR will be incorrect */
    writeb(0x01, ioaddr + FET_MIIADR);

    /* restore MII Data & turn on MAUTO*/
    writeb(byMIICRbak | 0x80, ioaddr + FET_MIICR);

    /* waiting for MDONE turn on. */
    for (ww = 0; ww < 0x3fff; ww++)
        if (readb(ioaddr + FET_MIIADR) & 0x20)
            break;

    /* turn on MSRCEN ater MDONE has turned on */    
    writeb( 0x40, ioaddr + FET_MIIADR);

    return wMII_DATA_REG;        
}

static void mdio_write(struct net_device *dev, int phy_id, int regnum, unsigned int value)
{
    unsigned long ioaddr = dev->base_addr;
    unsigned char byMIICRbak;
    netdev_private *np = (netdev_private *)dev->priv;
    int ww = 0;    

    if (phy_id == np->phy_addr) {
		switch (regnum) {
//		case 0:				
//			if (value & 0x9000)	
//				np->duplex_lock = 0;
//			else
//				np->full_duplex = (value & 0x0100) ? 1 : 0;
//			break;
		case 4: np->advertising = value;
		        break;
		}
	}

    /* backup MIICR */
    byMIICRbak = readb(ioaddr + FET_MIICR);

    /* write PHY id */
    writeb(phy_id, ioaddr + FET_MIICFG);

    /* before write MII data, we must turn off MAUTO */
    SafeDisableMiiAuto(dev);

    /* write MII register's ddress */
    writeb(regnum, ioaddr + FET_MIIADR);

    /* write MII register's data */
    writew(value, ioaddr + FET_MII_DATA_REG);

    /* Trigger writing */
    writeb(readb(ioaddr+FET_MIICR) | 0x20, ioaddr+FET_MIICR);

    /* waiting for write complete. */
    for (ww = 0; ww < 0x3fff; ww++)
        if (!(readb(ioaddr + FET_MIICR) & 0x20))
            break;

    // for VT3043 only
    if (np->revision < 0x20)
        mdelay(1);

    /* the value of MAD4-MAD0 bit must be 00001 before we turn on MAUTO */
    /* or the LNKFL bit in MIISR will be incorrect */
    writeb( 0x01, ioaddr + FET_MIIADR);

    /* restore MII Data & turn on MAUTO*/
    writeb(byMIICRbak | 0x80, ioaddr + FET_MIICR);

    /* waiting for MDONE turn on. */
    for (ww = 0; ww < 0x3fff; ww++)
        if (readb(ioaddr + FET_MIIADR) & 0x20)
            break;

    /* turn on MSRCEN ater MDONE has turned on */    
    writeb( 0x40, ioaddr + FET_MIIADR);

    return;
}

#ifdef CAM_data_read
//if select_CAM=0, read MCAM , else if slect_CAM=1, read VCAM
static void CAM_data_read(struct net_device *dev, int select_CAM, unsigned char CAM_address, unsigned char *value)
{
    unsigned long ioaddr = dev->base_addr;
    int uu;
    unsigned char FET_CAMC_temp;

    // invalid address
    if (CAM_address & 0xE0)
        printk("%s: the CAM address is invalid.\n", dev->name);

    // enable/select CAM controller
    FET_CAMC_temp = CAMC_CAMEN | (select_CAM ? CAMC_VCAMSL : 0);
    writeb(FET_CAMC_temp, ioaddr + FET_CAMC);

    // set CAM entry address
    writeb(CAM_address, ioaddr + FET_CAMADD);

    // issue read command
    writeb(CAMC_CAMRD | FET_CAMC_temp, ioaddr + FET_CAMC);

    // wait 2us for read completed
    udelay(2);

    if (select_CAM == CAMC_SELECT_VCAM) {
        // read VID CAM data
        *((unsigned int *)value)= readw(ioaddr + FET_VCAMD0);
    }
    else {
        // read Multicast CAM data
        for (uu = 0; uu < 6; uu++)
            *(value + uu)=readb(ioaddr + FET_MCAMD0 + uu);            
    }
    
    // disable CAMEN and return TRUE
    writeb(0, ioaddr + FET_CAMC);

    return;

}
#endif

//if select_CAM=0, write MCAM , else if slect_CAM=1, write VCAM
static void CAM_data_write(struct net_device *dev, int select_CAM, unsigned char CAM_address, unsigned char *value)
{
    unsigned long ioaddr = dev->base_addr;
    int uu;
    unsigned char FET_CAMC_temp;

    // invalid address
    if (CAM_address & 0xE0)
        printk("%s: the CAM address is invalid.\n", dev->name);

    // enable/select CAM controller
    FET_CAMC_temp = CAMC_CAMEN | (select_CAM ? CAMC_VCAMSL : 0);
    writeb(FET_CAMC_temp, ioaddr + FET_CAMC);

    // set CAM entry address
    writeb(CAM_address, ioaddr + FET_CAMADD);
 
    if (select_CAM == CAMC_SELECT_VCAM) {
        // read VID CAM data
        writew(*((unsigned int *)value), ioaddr + FET_VCAMD0);
    }
    else {
        // read Multicast CAM data
        for (uu = 0; uu < 6; uu++)
            writeb(*(value + uu), ioaddr + FET_MCAMD0 + uu);
    }
    // issue write command
    writeb(CAMC_CAMWR | FET_CAMC_temp , ioaddr + FET_CAMC);

    // wait 1us before next CAMWR    
    udelay(1);

    // disable CAMEN and return TRUE
    writeb(0, ioaddr + FET_CAMC);
}

#ifdef CAM_mask_read
//if select_CAM=0, read MCAM mask, else if slect_CAM=1, read VCAM mask
static unsigned int CAM_mask_read(struct net_device *dev, int select_CAM)
{
    unsigned long ioaddr = dev->base_addr;
    unsigned int mask_temp;

    // enable CAMEN
    writeb(CAMC_CAMEN | (select_CAM ? CAMC_VCAMSL : 0), ioaddr + FET_CAMC);

    // read mask       
    mask_temp = readl(ioaddr + FET_CAMMSK);
    
    // disable CAMEN
    writeb(0, ioaddr + FET_CAMC);
    return mask_temp;
}
#endif

//if select_CAM=0, write MCAM mask, else if slect_CAM=1, write VCAM mask
static void CAM_mask_write(struct net_device *dev, int select_CAM, unsigned int mask)
{
    unsigned long ioaddr = dev->base_addr;

    // enable CAMEN
    writeb(CAMC_CAMEN | (select_CAM ? CAMC_VCAMSL : 0), ioaddr + FET_CAMC);
       
    // write mask
    writel(mask, ioaddr + FET_CAMMSK);
    
    // disable CAMEN
    writeb(0, ioaddr + FET_CAMC);
}

#ifdef reset
/* reset the chip */
static void netdev_reset(struct net_device *dev)
{
    unsigned long ioaddr = dev->base_addr;
    int ww;

    /* Reset the chip. */
    writew(FET_CmdReset | readw(ioaddr + FET_CR0), ioaddr + FET_CR0);
    // polling till software reset complete
    // W_MAX_TIMEOUT is the timeout period
    for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
        if ((readw(ioaddr + FET_CR0) & FET_CmdReset) == 0 )
            break;
    }
}
#endif

static void VLAN_tagging(struct net_device *dev)
{
    netdev_private *np = (netdev_private *)dev->priv;
    unsigned long ioaddr = dev->base_addr;
    unsigned int VCAM_temp = 0;

    VCAM_temp= (enable_tagging[np->card_idx] == 1 && (VID_setting[np->card_idx] < 4095) && (VID_setting[np->card_idx] > 0))
                   ? VID_setting[np->card_idx] : 0;
 //   printk("chenyp:VCAM_temp=%x\n", VCAM_temp);

    // set {PQEN, RTGOPT} = {1,0} in TCR
    // in this mode, tx: all packet tagged,  rx: both untagged/tagged packets
    writeb((readb(ioaddr + FET_TCR) & 0xEF) | 0x01, ioaddr + FET_TCR);

    // Disable tagging
    if (enable_tagging[np->card_idx] != 1) {
        // VLAN CAM[0]= 0
        CAM_data_write(dev, 1, 0, (unsigned char *)&VCAM_temp);

        // VLAN CAM mask = 1
        CAM_mask_write(dev,1,0x00000001);
    }
    else {
        // Enable tagging, no VLAN setting
        if (VCAM_temp == 0) {
            // VLAN CAM[0]= 0
            CAM_data_write(dev, 1, 0, (unsigned char *)&VCAM_temp);

            // VLAN CAM mask = 1
            CAM_mask_write(dev,1,0x00000001);

        }
        // Enable tagging, VLAN setting to single or multiple VID and VID !=0
        else {
            // VLAN CAM[0]= user defined value
            CAM_data_write(dev, 1, 0, (unsigned char *)&VCAM_temp);

            // VLAN CAM mask = 1
            CAM_mask_write(dev,1,0x00000001);
        }
    }
    // set VIDFR =1 in BCR1, VLAN ID hardware filtering.
    writeb(readb(ioaddr + FET_BCR1) | 0x80, ioaddr + FET_BCR1);

}
static void turn_on_MII_link(struct net_device *dev)
{
    unsigned long ioaddr = dev->base_addr;
    unsigned int MIICRbak;
    int ww;

    MIICRbak = readb(ioaddr + FET_MIICR);
    SafeDisableMiiAuto(dev);
    writeb(0x01, ioaddr + FET_MIIADR);
    outb(MIICRbak | 0x80, ioaddr + FET_MIICR);
    for (ww = 0; ww < 0x3fff; ww++)
    if (readb(ioaddr + FET_MIIADR) & 0x20)
        break;
    /* turn on MSRCEN ater MDONE has turned on */    
    writeb(0x40, ioaddr + FET_MIIADR);
    mdelay(3);
}
static int netdev_open(struct net_device *dev)
{
    netdev_private *np = (netdev_private *)dev->priv;
    unsigned long ioaddr = dev->base_addr;
    int i;

//    netdev_reset(dev);

    MOD_INC_USE_COUNT;

    if (request_irq(dev->irq, &intr_handler, SA_SHIRQ, dev->name, dev)) {

        MOD_DEC_USE_COUNT;
        return -EAGAIN;
    }

    if (debug > 1)
        printk(KERN_INFO "%s: netdev_open() irq %d.\n",
               dev->name, dev->irq);
    init_ring(dev);
    writel(virt_to_bus(np->rx_ring), ioaddr + FET_CURR_RX_DESC_ADDR);
//    printk("chenyp:readl(np->rx_ring)=%x\n",readl(ioaddr + FET_CURR_RX_DESC_ADDR));
    if (np->revision >= 0x80 && enable_tagging[np->card_idx] == 1) {
        for (i = 0; i < 8; i++) {
    	    writel(virt_to_bus(np->tx_ring[i]), ioaddr + FET_CURR_TX_DESC_ADDR + ((7 - i) * 4));
 //           printk("chenyp:virt_to_bus(np->tx_ring[ %d ])=%xreadl(np->tx_ring %d)=%x\n",i,virt_to_bus(np->tx_ring[i]),i,ioaddr + FET_CURR_TX_DESC_ADDR + (i * 4));

        }
    }
    else /* np->revision < 0x80 */
    	writel(virt_to_bus(np->tx_ring[0]), ioaddr + FET_CURR_TX_DESC_ADDR);
    for (i = 0; i < 6; i++)
        writeb(dev->dev_addr[i], ioaddr + FET_PAR + i);

    /* Initialize other registers. */    
    // Turn on bit3 (OFSET) in TCR during MAC initialization.
    writeb(readb(ioaddr + FET_TCR) | FET_TCR_OFSET, ioaddr + FET_TCR);

    /* turn on MII link change */
    /* if the MAD4-0 is not 0x0001, then Link Fail will be on */
    turn_on_MII_link(dev);

#if (LINUX_VERSION_CODE >= 0x02032a)
    netif_start_queue(dev);
#else /* (LINUX_VERSION_CODE >= 0x02032a) */
    dev->tbusy = 0;
    dev->interrupt = 0;
    dev->start = 1;
#endif /* (LINUX_VERSION_CODE >= 0x02032a) */

    /* Configure the FIFO thresholds. */
    np->tx_thresh = (tx_thresh[np->card_idx] >= 0 && tx_thresh[np->card_idx] <= 4) ? tx_thresh[np->card_idx]: 0;
    np->rx_thresh = (rx_thresh[np->card_idx] >= 0 && rx_thresh[np->card_idx] <= 7) ? rx_thresh[np->card_idx]: 0;
    np->DMA_length = (DMA_length[np->card_idx] >= 0 && DMA_length[np->card_idx] <= 7) ? DMA_length[np->card_idx]: 1;

    // Set rx threshold, default = 64 bytes
    writeb((readb(ioaddr + FET_BCR0) & 0xC7) | (np->rx_thresh << 3), ioaddr + FET_BCR0);
    writeb((readb(ioaddr + FET_RCR) & 0x1F) | (np->rx_thresh << 5), ioaddr + FET_RCR);

    // Set tx threshold, default = 128 bytes
    writeb((readb(ioaddr + FET_BCR1) & 0xC7) | (np->tx_thresh << 3), ioaddr + FET_BCR1);
    writeb((readb(ioaddr + FET_TCR) & 0x1F) | (np->tx_thresh << 5), ioaddr + FET_TCR);

    // Set DMA length, default = 16 DWORDs = 64 bytes
    writeb((readb(ioaddr + FET_BCR0) & 0xF8) | np->DMA_length , ioaddr + FET_BCR0);
   
    set_rx_mode(dev);

    if (np->revision >= 0x80)
        set_flow_control(dev);

    set_media_duplex_mode(dev);

    if (np->revision >= 0x80) {
        /* For non-blocking priority transmit, set {TXQBKT1, TXQBKT0} -> {1,1} */
        /* So, the Tx Queue Block Counter = 8 packets*/
        writeb(readb(ioaddr + FET_GFTEST) | 0x30, ioaddr + FET_GFTEST);
        /* Set non-blocking priority transmit*/
        writeb(readb(ioaddr + FET_BCR1) | 0x40, ioaddr + FET_BCR1);
    }
    if (np->revision >= 0x40)
       flow_control_ability (dev);

    /* 802.1p/Q tagging user setting and behavior */
    if (np->revision >= 0x80)
        VLAN_tagging(dev);



    /* Enable interrupts by setting the interrupt mask. */
    writew(FET_ISR_PRX | FET_ISR_PTX | FET_ISR_RXE | FET_ISR_TXE | 
           FET_ISR_TU | FET_ISR_RU | FET_ISR_BE | FET_ISR_CNT |
           FET_ISR_ERI | FET_ISR_UDFI | FET_ISR_OVFI | FET_ISR_PKTRACE | 
           FET_ISR_NORBF | FET_ISR_ABTI | FET_ISR_SRCI ,
           ioaddr + FET_IMR0);

    np->chip_cmd = FET_CmdStart|FET_CmdTxOn|FET_CmdRxOn|FET_CmdNoTxPoll;
    if (np->full_duplex)
        np->chip_cmd |= FET_CmdFDuplex;
    writeb((np->chip_cmd & 0xff), ioaddr + FET_CR0);
    writeb( (np->chip_cmd >>8) |readb(ioaddr + FET_CR1), ioaddr + FET_CR1);
    
    /* The LED outputs of various MII xcvrs should be configured.  */
    /* For NS or Mison phys, turn on bit 1 in register 0x17 */
    /* For ESI phys, turn on bit 7 in register 0x17. */
    mdio_write(dev, np->phy_addr, 0x17, mdio_read(dev, np->phy_addr, 0x17) |
               (np->drv_flags & FET_HasESIPhy) ? 0x0080 : 0x0001);

    if (debug > 2)
        printk(KERN_INFO "%s: Done netdev_open(), status %4.4x "
               "MII status: %4.4x.\n",
               dev->name, readw(ioaddr + FET_CR0),
               mdio_read(dev, np->phy_addr, 1));
    /* Set the timer to check for link beat. */
    //init_timer(&np->timer);
    //np->timer.expires = jiffies + 2;
    //np->timer.data = (unsigned long)dev;
    //np->timer.function = &netdev_timer;             /* timer handler */
    //add_timer(&np->timer);
//printk("chenyp:netdev_open:phy0=%x, phy1=%x, phy2=%x, phy3=%x, phy4=%x, phy5=%x, mac8=%x, mac99=%x mac80=%x\n",mdio_read(dev,np->phy_addr,0),mdio_read(dev,np->phy_addr,1),mdio_read(dev,np->phy_addr,2),mdio_read(dev,np->phy_addr,3),mdio_read(dev,np->phy_addr,4),mdio_read(dev,np->phy_addr,5),readb(ioaddr+0x08),readb(ioaddr+0x99),readb(ioaddr+0x80));

    return 0;
}

static void do_autonegotiation(struct net_device *dev)
{
    netdev_private *np = (netdev_private *)dev->priv;
    unsigned long ioaddr = dev->base_addr;

    /* check whether link fail */
    if (readb(ioaddr + FET_MIISR) & 0x02){
        printk(KERN_INFO "%s: Link Fail.\n",dev->name);
        np->full_duplex = 0;
        np->chip_cmd &= FET_CmdNoFDuplex;
        return;
    }
    /* check speed*/
    /* read N_SPD10 bit in MII Status Register */
    if (readb(ioaddr + FET_MIISR) & 0x01)
        printk(KERN_INFO "%s: Autonegotiation result: 10Mbps", dev->name);
    else
        printk(KERN_INFO "%s: Autonegotiation result: 100Mbps", dev->name);
    /* check duplex mode*/
    /* if VT3106, check N_FDX bit in MII Status Register directly */
    if (np->revision >= 0x80) {
        if (readb(ioaddr + FET_MIISR) & 0x04) {
            printk(" full duplex mode.\n");
            /* Set MAC operating in Full Duplex Mode*/
            writew(readw(ioaddr + FET_CR0) | 0x0400, ioaddr + FET_CR0);
            np->full_duplex = 1;
            np->chip_cmd |= FET_CmdFDuplex;
        }
        else {
            printk(" half duplex mode.\n");
            /* Set MAC operating in Half Duplex Mode*/
            writew(readw(ioaddr + FET_CR0) & 0xfbff, ioaddr + FET_CR0);
            np->full_duplex = 0;
            np->chip_cmd &= FET_CmdNoFDuplex;
        }
    }
    else {
        /* if VT3065 or VT3043, check ANAR and ANLPAR in MII Registers of PHY */
        if ((mdio_read(dev, np->phy_addr, 4) & mdio_read(dev, np->phy_addr, 5)) & 0x0140 ) {
            printk(" full duplex mode.\n");
            /* Set MAC operating in Full Duplex Mode*/
            writew(readw(ioaddr + FET_CR0) | 0x0400, ioaddr + FET_CR0);
            np->full_duplex = 1;
            np->chip_cmd |= FET_CmdFDuplex;
        }
        else {
            printk(" half duplex mode.\n");
            /* Set MAC operating in Half Duplex Mode*/
            writew(readw(ioaddr + FET_CR0) & 0xfbff, ioaddr + FET_CR0);
            np->full_duplex = 0;
            np->chip_cmd &= FET_CmdNoFDuplex;
        }
    }
}

/* Do lagacy force if in force mode*/
static void check_legacy_force(struct net_device *dev)
{
    netdev_private *np = (netdev_private *)dev->priv;
    unsigned long ioaddr = dev->base_addr;
    unsigned int FET_BCR1_temp = 0;

//printk("chenyp:legacy_force\n");
    /* If MEDEN bit in CFGC is on, then it's forced mode, 
       otherwise, it use autonegotiation. Only for VT3065 and VT3043 */
    if(np->revision < 0x80 && readb(ioaddr + FET_CFGC) & 0x80){
        /* if MED2 bit in BCR0 is on, then it use autonegotiation*/
        if (readb(ioaddr + FET_BCR0) & 0x80)
            np->auto_negotiation = 1;
        else {
            np->auto_negotiation = 0;

            FET_BCR1_temp = readb(ioaddr + FET_BCR1);
            FET_BCR1_temp = FET_BCR1_temp & 0xC0 ;

            /* Disable autonigotiation */
            mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) & 0xEFFF);

            /* set loopback in MII to un-link in 100M mode, */
            /* in 10M mode set this bit cannot make it un-link */
            /* but it doesn't matter */
            mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) | 0x4000);

            if (FET_BCR1_temp == 0x00) {
                printk(KERN_INFO "%s: Force to 10Mbps Half duplex mode.\n", dev->name);
                /* Set speed 10Mbps */
                mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) & 0xDFFF);
                /* Set half duplex */
                mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) & 0xFEFF);
                /* Set MAC operating in Half Duplex Mode*/
                writew(readw(ioaddr + FET_CR0) & 0xfbff, ioaddr + FET_CR0);
                np->full_duplex = 0;
                np->chip_cmd &= FET_CmdNoFDuplex;
            }
            else if (FET_BCR1_temp == 0x40) {
                printk(KERN_INFO "%s: Force to 100Mbps Half duplex mode.\n", dev->name);
                /* Set speed 100Mbps */
                mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) | 0x2000);
                /* Set half duplex */
                mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) & 0xFEFF);
                /* Set MAC operating in Half Duplex Mode*/
                writew(readw(ioaddr + FET_CR0) & 0xfbff, ioaddr + FET_CR0);
                np->full_duplex = 0;
                np->chip_cmd &= FET_CmdNoFDuplex;
            }
            else if (FET_BCR1_temp == 0x80) {
                printk(KERN_INFO "%s: Force to 10Mbps Full duplex mode.\n", dev->name);
                /* Set speed 10Mbps */
                mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) & 0xDFFF);
                /* Set full duplex */
                mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) | 0x0100);
                /* Set MAC operating in Full Duplex Mode*/
                writew(readw(ioaddr + FET_CR0) | 0x0400, ioaddr + FET_CR0);
                np->full_duplex = 1;
                np->chip_cmd |= FET_CmdFDuplex;
            }
            else if (FET_BCR1_temp == 0xC0) {
                printk(KERN_INFO "%s: Force to 100Mbps Full duplex mode.\n", dev->name);
                /* Set speed 100Mbps */
                mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) | 0x2000);
                /* Set full duplex */
                mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) | 0x0100);
                /* Set MAC operating in Full Duplex Mode*/
                writew(readw(ioaddr + FET_CR0) | 0x0400, ioaddr + FET_CR0);
                np->full_duplex = 1;
                np->chip_cmd |= FET_CmdFDuplex;
            }
            /* delay to avoid link down from force-10M to force-100M */
            mdelay(300);

            /* clear LPBK bit in BMCR register to re-link */
            mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) & 0xBFFF);
        }
    }
    else {
        /* Check whether user define the speed and duplex mode */
        if (np->card_idx < MAX_UNITS  &&  speed_duplex[np->card_idx] > 0 && speed_duplex[np->card_idx] <= 4) {
            np->auto_negotiation = 0;

            /* Disable autonigotiation */
            mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) & 0xEFFF);

            /* set loopback in MII to un-link in 100M mode, */
            /* in 10M mode set this bit cannot make it un-link */
            /* but it doesn't matter */
            mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) | 0x4000);

            if (speed_duplex[np->card_idx] == 1) {
                printk(KERN_INFO "%s: Force to 100Mbps Half duplex mode.\n", dev->name);
                /* Set speed 100Mbps */
                mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) | 0x2000);
                /* Set half duplex */
                mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) & 0xFEFF);
                /* Set MAC operating in Half Duplex Mode*/
                writew(readw(ioaddr + FET_CR0) & 0xfbff, ioaddr + FET_CR0);
                np->full_duplex = 0;
                np->chip_cmd &= FET_CmdNoFDuplex;
            }
            else if (speed_duplex[np->card_idx] == 2) {
                printk(KERN_INFO "%s: Force to 100Mbps Full duplex mode.\n", dev->name);
                /* Set speed 100Mbps */
                mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) | 0x2000);
                /* Set full duplex */
                mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) | 0x0100);
                /* Set MAC operating in Full Duplex Mode*/
                writew(readw(ioaddr + FET_CR0) | 0x0400, ioaddr + FET_CR0);
                np->full_duplex = 1;
                np->chip_cmd |= FET_CmdFDuplex;
            }
            else if (speed_duplex[np->card_idx] == 3) {
                printk(KERN_INFO "%s: Force to 10Mbps Half duplex mode.\n", dev->name);
                /* Set speed 10Mbps */
                mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) & 0xDFFF);
                /* Set half duplex */
                mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) & 0xFEFF);
                /* Set MAC operating in Half Duplex Mode*/
                writew(readw(ioaddr + FET_CR0) & 0xfbff, ioaddr + FET_CR0);
                np->full_duplex = 0;
                np->chip_cmd &= FET_CmdNoFDuplex;
            }
            else if (speed_duplex[np->card_idx] == 4) {
                printk(KERN_INFO "%s: Force to 10Mbps Full duplex mode.\n", dev->name);
                /* Set speed 10Mbps */
                mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) & 0xDFFF);
                /* Set full duplex */
                mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) | 0x0100);
                /* Set MAC operating in Full Duplex Mode*/
                writew(readw(ioaddr + FET_CR0) | 0x0400, ioaddr + FET_CR0);
                np->full_duplex = 1;
                np->chip_cmd |= FET_CmdFDuplex;
            }
            /* delay to avoid link down from force-10M to force-100M */
            mdelay(300);

            /* clear LPBK bit in BMCR register to re-link */
            mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) & 0xBFFF);
        }
        else
            np->auto_negotiation = 1;
    }
}

/* Restart autonegotiation */
static void restart_autonegotiation(struct net_device *dev)
{
    netdev_private *np = (netdev_private *)dev->priv;
    unsigned int i;

    /* Restart autonegotiation */
    mdio_write(dev, np->phy_addr, 0, mdio_read (dev, np->phy_addr, 0) | 0x0200);
    /* Wait until N-WAY finished*/
    mdelay(2500); /* delay for 2.5 seconds */
    for (i=0; i<0x1ff; i++)
        if (mdio_read(dev, np->phy_addr, 1) & 0x0020)
            break;
}

/* Turn on AUTO bit in MII regiser */
static void enable_autonegotiation(struct net_device *dev)
{
    netdev_private *np = (netdev_private *)dev->priv;
    unsigned int i;
    unsigned int PHY_BMCR_temp = 0;

    PHY_BMCR_temp = mdio_read(dev, np->phy_addr, 0);

    /* Set Autonegotiation enable (ANEG_EN bit in Control register ) */
    mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) | 0x1000);

    /* Forced mode to auto mode, it will cause autonegotiation restart */
    /* Wait until N-WAY finished*/
    if (!( PHY_BMCR_temp & 0x1000)) {
        mdelay(2300);
        for (i=0; i<0x1ff; i++)
            if (mdio_read(dev, np->phy_addr, 1) & 0x0020)
                break;
    }
}

/* Do N-WAY force if in force mode */
static int check_n_way_force (struct net_device *dev, int change_flag)
{
    netdev_private *np = (netdev_private *)dev->priv;
    unsigned long ioaddr = dev->base_addr;
    unsigned int MII_BMCR_temp = 0, MII_ANAR_temp=0;
//printk("chenyp:n_way_force\n");
    /* Read original BMCR and ANAR value from MII */
    MII_BMCR_temp = mdio_read(dev, np->phy_addr, 0) ;
    MII_ANAR_temp = mdio_read(dev, np->phy_addr, 4) & 0x01E0;

    /* Check whether user define the speed and duplex mode */
    if (np->card_idx < MAX_UNITS  &&  speed_duplex[np->card_idx] > 0 && speed_duplex[np->card_idx] <= 4) {
        np->auto_negotiation = 0;

        /* Force to 100Mbps Half duplex */
        if (speed_duplex[np->card_idx] == 1) {
            printk(KERN_INFO "%s: Force to 100Mbps Half duplex mode.\n", dev->name);
            /* Compare user defined mode with original ANAR value*/
            /* If the setting is the same, do nothing, or we msut write the new setting to ANAR */
            if (MII_ANAR_temp != 0x0080) {
                change_flag |= 1;
                /*Write the new setting to ANAR */
                mdio_write(dev, np->phy_addr, 4, (mdio_read(dev, np->phy_addr, 4) & 0xFE1F) | 0x0080);
//                printk("chenyp:mdio_read(dev, np->phy_addr, 4)=%x\n", mdio_read(dev, np->phy_addr, 4));
            }

            /* Set speed 100Mbps */
            mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) | 0x2000);
            /* Set half duplex */
            mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) & 0xFEFF);

            /* If AUTO bit is on and the setting is changed, issue REAUTO in BMCR */
            if ((MII_BMCR_temp & 0x1000) && (change_flag == 1))
                restart_autonegotiation(dev);
            else if (!(MII_BMCR_temp & 0x1000))
                enable_autonegotiation(dev);

            /* Set MAC operating in Half Duplex Mode*/
            writew(readw(ioaddr + FET_CR0) & 0xfbff, ioaddr + FET_CR0);
            np->full_duplex = 0;
            np->chip_cmd &= FET_CmdNoFDuplex;
        }
        /* Force to 100Mbps Full duplex mode */
        else if (speed_duplex[np->card_idx] == 2) {
            printk(KERN_INFO "%s: Force to 100Mbps Full duplex mode.\n", dev->name);
            /* Compare user defined mode with original ANAR value*/
            /* If the setting is the same, do nothing, or we must write the new setting to ANAR */
            if (MII_ANAR_temp != 0x0100) {
                change_flag |= 1;
                /*Write the new setting to ANAR */
                mdio_write(dev, np->phy_addr, 4, (mdio_read(dev, np->phy_addr, 4) & 0xFE1F) | 0x0100);
//                printk("chenyp:mdio_read(dev, np->phy_addr, 4)=%x\n", mdio_read(dev, np->phy_addr, 4));
            }

            /* Set speed 100Mbps */
            mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) | 0x2000);
            /* Set full duplex */
            mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) | 0x0100);

            /* If AUTO bit is on and the setting is changed, issue REAUTO in BMCR */
            if ((MII_BMCR_temp & 0x1000) && (change_flag == 1))
                restart_autonegotiation(dev);
            else if (!(MII_BMCR_temp & 0x1000))
                enable_autonegotiation(dev);

            /* Set MAC operating in Full Duplex Mode*/
            writew(readw(ioaddr + FET_CR0) | 0x0400, ioaddr + FET_CR0);
            np->full_duplex = 1;
            np->chip_cmd |= FET_CmdFDuplex;
        }
        /* Force to 10Mbps Half duplex mode */
        else if (speed_duplex[np->card_idx] == 3) {
            printk(KERN_INFO "%s: Force to 10Mbps Half duplex mode.\n", dev->name);
            /* Compare user defined mode with original ANAR value*/
            /* If the setting is the same, do nothing, or we msut write the new setting to ANAR */
            if (MII_ANAR_temp != 0x0020) {
                change_flag |= 1;
                /*Write the new setting to ANAR */
                mdio_write(dev, np->phy_addr, 4, (mdio_read(dev, np->phy_addr, 4) & 0xFE1F) | 0x0020);
//                printk("chenyp:mdio_read(dev, np->phy_addr, 4)=%x\n", mdio_read(dev, np->phy_addr, 4));
            }
            /* Set speed 10Mbps */
            mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) & 0xDFFF);
            /* Set half duplex */
            mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) & 0xFEFF);

            /* If AUTO bit is on and the setting is changed, issue REAUTO in BMCR */
            if ((MII_BMCR_temp & 0x1000) && (change_flag == 1))
                restart_autonegotiation(dev);
            else if (!(MII_BMCR_temp & 0x1000))
                enable_autonegotiation(dev);

            /* Set MAC operating in Half Duplex Mode*/
            writew(readw(ioaddr + FET_CR0) & 0xfbff, ioaddr + FET_CR0);
            np->full_duplex = 0;
            np->chip_cmd &= FET_CmdNoFDuplex;
        }
        /* Force to 10Mbps Full duplex mode */
        else if (speed_duplex[np->card_idx] == 4) {
            printk(KERN_INFO "%s: Force to 10Mbps Full duplex mode.\n", dev->name);
            /* Compare user defined mode with original ANAR value*/
            /* If the setting is the same, do nothing, or we msut write the new setting to ANAR */
            if (MII_ANAR_temp != 0x0040) {
                change_flag |= 1;
                /*Write the new setting to ANAR */
                mdio_write(dev, np->phy_addr, 4, (mdio_read(dev, np->phy_addr, 4) & 0xFE1F) | 0x0040);
//                printk("chenyp:mdio_read(dev, np->phy_addr, 4)=%x\n", mdio_read(dev, np->phy_addr, 4));
            }

            /* Set speed 10Mbps */
            mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) & 0xDFFF);
            /* Set full duplex */
            mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) | 0x0100);

            /* If AUTO bit is on and the setting is changed, issue REAUTO in BMCR */
            if ((MII_BMCR_temp & 0x1000) && (change_flag == 1))
                restart_autonegotiation(dev);
            else if (!(MII_BMCR_temp & 0x1000))
                enable_autonegotiation(dev);

            /* Set MAC operating in Full Duplex Mode*/
            writew(readw(ioaddr + FET_CR0) | 0x0400, ioaddr + FET_CR0);
            np->full_duplex = 1;
            np->chip_cmd |= FET_CmdFDuplex;
        }
    }
    else
        np->auto_negotiation = 1;
    return change_flag;
}

static void set_media_duplex_mode(struct net_device *dev)
{
    netdev_private *np = (netdev_private *)dev->priv;
    int change_flag =0;
    unsigned long ioaddr = dev->base_addr;
    /* record whether the ANAR value is changed, then we need trigger N-WAY*/


    /* For flow control , according to user defined flow control option, */
    /* to set the PAUSE ability in PHY's ANAR register*/
    /* Only for VT3065 and VT3106*/
    if (np->revision >= 0x40 && flow_control[np->card_idx] != 1) {
        unsigned int MII_ANAR_temp=0;

        /* Backup the value of PHY's ANAR register */
        MII_ANAR_temp = mdio_read(dev, np->phy_addr, 4);

        /* Disable PAUSE in ANAR*/
        if (flow_control[np->card_idx] == 2) {
            /* Check whether the PUASE has already be disabled*/
            /* If not, disable it*/
            if(MII_ANAR_temp & 0x0400) {
                change_flag = 1;
                mdio_write(dev, np->phy_addr, 4, MII_ANAR_temp & 0xFBFF);
            }
        }
        /* Enable PAUSE in ANAR*/
        if (flow_control[np->card_idx] == 3) {
            /* Check whether the PUASE has already be enabled*/
            /* If not, enable it*/
            if(!(MII_ANAR_temp & 0x0400)) {
                change_flag = 1;
                mdio_write(dev, np->phy_addr, 4, MII_ANAR_temp | 0x0400);
            }
        }
    }     
    if (np->revision >= 0x80 || (np->revision >= 0x40 && np->revision < 0x80 && !(readb(ioaddr + FET_CFGC) & 0x80)))
        change_flag=check_n_way_force(dev, change_flag);
    else
        check_legacy_force(dev);

    /* For N-WAY force in VT3106 or VT3072 phy */
    /* Make sure the PHY is VT3106's PHY or VT3072*/
    /* So we check PHY REG 'h3 (PHY Identifier1) bit[9:4] is 6'b110100*/
    if((mdio_read(dev, np->phy_addr, 3) & 0x03f0) == 0x0340 ||
      ((mdio_read(dev, np->phy_addr, 3) & 0x03f0) == 0x0320 && 
       (mdio_read(dev, np->phy_addr, 3) & 0x000f) >= 5)) {
//      printk("chenyp:yes,it's 3072 phy here\n");
        /* if forced mode, turn on bit 0 else turn off bit 0 in MII 0x10 register */
        if(np->auto_negotiation == 0 && (np->revision >= 0x80 || (np->revision >= 0x40 && np->revision < 0x80 && !(readb(ioaddr + FET_CFGC) & 0x80))))
            /* write PHY REG 'h10(PHY MODE CONFIG) bit[0] as 1'b1 */
            mdio_write(dev, np->phy_addr, 0x10, mdio_read(dev, np->phy_addr, 0x10) | 0x0001);
        else
            /* write PHY REG 'h10(PHY MODE CONFIG) bit[0] as 1'b0 */
            mdio_write(dev, np->phy_addr, 0x10, mdio_read(dev, np->phy_addr, 0x10) & 0xfffe);
    }
//printk("chenyp:set_media_force_mode:after force:phy0=%x, phy1=%x, phy2=%x, phy3=%x, phy4=%x, phy5=%x, mac8=%x, mac9=%x mac6d=%x\n",mdio_read(dev,np->phy_addr,0),mdio_read(dev,np->phy_addr,1),mdio_read(dev,np->phy_addr,2),mdio_read(dev,np->phy_addr,3),mdio_read(dev,np->phy_addr,4),mdio_read(dev,np->phy_addr,5),readb(ioaddr+0x08),readb(ioaddr+0x09),readb(ioaddr+0x6d));

    if (np->auto_negotiation == 1) {
        unsigned int PHY_BMCR_temp = 0;
        unsigned int PHY_ANAR_temp = 0;
        int restart_auto = 0;
        int i;

        PHY_BMCR_temp = mdio_read(dev, np->phy_addr, 0);
        PHY_ANAR_temp = mdio_read(dev, np->phy_addr, 4);

        /* Set Autonegotiation enable (ANEG_EN bit in Control register ) */
        mdio_write(dev, np->phy_addr, 0 , mdio_read(dev, np->phy_addr, 0) | 0x1000);

        // check the ANAR value is correct
        // if not, write back the correct value, and retrigger Autonegotiation
        if ((PHY_ANAR_temp & 0x01E0) != 0x01E0) {
            mdio_write(dev, np->phy_addr, 4, PHY_ANAR_temp | 0x01E0);
            restart_auto = 1;
        }

        /* for VT3043(DAVICOM) only, fix DaviCom PHY's bug */
        if (np->revision < 0x20 ) {
            unsigned int phy_ID;

            phy_ID = (mdio_read(dev, np->phy_addr, 2) << 16) | (mdio_read(dev, np->phy_addr, 3));
            if (phy_ID >= CID_DAVICOM && phy_ID < CID_DAVICOM_B)
                restart_auto = 1;
        }
        if (restart_auto == 1)
            restart_autonegotiation(dev);
        /* Forced mode to auto mode, it will cause autonegotiation restart */
        /* Wait until N-WAY finished*/
        else if (!( PHY_BMCR_temp & 0x1000) && !(mdio_read(dev, np->phy_addr, 1) & 0x0020)) {
            mdelay(2300);
            for (i=0; i<0x1ff; i++)
                if (mdio_read(dev, np->phy_addr, 1) & 0x0020)
                    break;
        }
        do_autonegotiation(dev);
    }
}
/* Set flow control capability accroding to ANAR and ANLPAR register in MII */
/* The half duplex flow control capability is turn off now, because it's not in the spec.*/
/* Follow the table 28B-3 in the IEEE Standard 802.3, 2000 Edition to set */
/* full duplex flow control capability*/
static void flow_control_ability(struct net_device *dev)
{
    netdev_private *np = (netdev_private *)dev->priv;
    unsigned long ioaddr = dev->base_addr;
    unsigned int PHYANAR_temp, PHYANLPAR_temp, MIISR_temp, FlowCR1_temp, Micr0_temp;

    if (np->revision >= 0x40 && np->revision < 0x80) {
       /* Read the old value of FlowCR1 register */
        Micr0_temp = readb(ioaddr + FET_Micr0);

        /*check whether NIC is operated in full duplex mode */
        /* in full duplex mode*/
        if (np->full_duplex == 1) {
            /* read PAUSE and ASM_DIR in PHYANAR and MIISR register*/
            PHYANAR_temp = (mdio_read(dev, np->phy_addr, 4) & 0x0C00) >> 10;
            PHYANLPAR_temp = (mdio_read(dev, np->phy_addr, 5) & 0x0C00) >> 10;

            /* Local: ASM_DIR=1, PAUSE=0   Remote: ASM_DIR=1, PAUSE=1*/
            if ( (PHYANAR_temp & 0x02) && (!(PHYANAR_temp & 0x01)) && (PHYANLPAR_temp & 0x02) && (PHYANLPAR_temp & 0x01)) {
                /* Disable PAUSE receive */
                Micr0_temp = Micr0_temp & 0xF7;
            }
            /* Local: ASM_DIR=Don't care, PAUSE=1   Remote: ASM_DIR=Don't care, PAUSE=1*/
            else if (PHYANAR_temp & 0x01 && PHYANLPAR_temp & 0x01) {
                /* Enable PAUSE receive */
                Micr0_temp = Micr0_temp | 0x08;
            }
            /* Local: ASM_DIR=1, PAUSE=1   Remote: ASM_DIR=1, PAUSE=0*/
            else if ( (PHYANAR_temp & 0x02) && (PHYANAR_temp & 0x01) && (PHYANLPAR_temp & 0x02) && (!(PHYANLPAR_temp & 0x01))) {
                /* Enable PAUSE receive */
                Micr0_temp = Micr0_temp | 0x08;
            }
            /* Other conditions*/
            else {
                /* Disable PAUSE receive */
                Micr0_temp = Micr0_temp & 0xF7;
            }
        }
        /* in half duplex mode*/
        else {
            /* Disable PAUSE receive */
            Micr0_temp = Micr0_temp & 0xF7;        
        }
        /* Disable half duplex flow control */
        Micr0_temp = Micr0_temp & 0xFB;

        /* Disable full duplex PAUSE transmit */
        Micr0_temp = Micr0_temp & 0xEF;

        /* Set the Micr0 register*/
        writeb( Micr0_temp , ioaddr + FET_Micr0);
    }
    else if (np->revision >= 0x80) {
        /* Read the old value of FlowCR1 register */
        FlowCR1_temp = readb(ioaddr + FET_FlowCR1);


        /*check whether NIC is operated in full duplex mode */
        /* in full duplex mode*/
        if (np->full_duplex == 1) {
            /* read PAUSE and ASM_DIR in PHYANAR and MIISR register*/
            PHYANAR_temp = (mdio_read(dev, np->phy_addr, 4) & 0x0C00) >> 10;
            MIISR_temp = (readb(ioaddr + FET_MIISR) & 0x60) >> 5;

            /* Local: ASM_DIR=1, PAUSE=0   Remote: ASM_DIR=1, PAUSE=1*/
            if ( (PHYANAR_temp & 0x02) && (!(PHYANAR_temp & 0x01)) && (MIISR_temp & 0x02) && (MIISR_temp & 0x01)) {
                /* Enable PAUSE transmit */
                FlowCR1_temp = FlowCR1_temp | 0x04;
                /* Disable PAUSE receive */
                FlowCR1_temp = FlowCR1_temp & 0xFD;
            }
            /* Local: ASM_DIR=Don't care, PAUSE=1   Remote: ASM_DIR=Don't care, PAUSE=1*/
            else if (PHYANAR_temp & 0x01 && MIISR_temp & 0x01) {
                /* Enable PAUSE transmit */
                FlowCR1_temp = FlowCR1_temp | 0x04;
                /* Enable PAUSE receive */
                FlowCR1_temp = FlowCR1_temp | 0x02;
            }
            /* Local: ASM_DIR=1, PAUSE=1   Remote: ASM_DIR=1, PAUSE=0*/
            else if ( (PHYANAR_temp & 0x02) && (PHYANAR_temp & 0x01) && (MIISR_temp & 0x02) && (!(MIISR_temp & 0x01))) {
                /* Disable PAUSE transmit */
                FlowCR1_temp = FlowCR1_temp & 0xFB;
                /* Enable PAUSE receive */
                FlowCR1_temp = FlowCR1_temp | 0x02;
            }
            /* Other conditions*/
            else {
                /* Disable PAUSE transmit */
                FlowCR1_temp = FlowCR1_temp & 0xFB;
                /* Disable PAUSE receive */
                FlowCR1_temp = FlowCR1_temp & 0xFD;
            }
        }
        /* in half duplex mode*/
        else {
            /* Disable PAUSE transmit */
            FlowCR1_temp = FlowCR1_temp & 0xFB;
            /* Disable PAUSE receive */
            FlowCR1_temp = FlowCR1_temp & 0xFD;        
        }
        /* Disable half duplex flow control */
        FlowCR1_temp = FlowCR1_temp & 0xFE;

        /* Set the FlowCR1 register*/
        writeb( FlowCR1_temp , ioaddr + FET_FlowCR1);
    }
}

static void set_flow_control(struct net_device *dev)
{
    unsigned long ioaddr = dev->base_addr;
    unsigned int temp_FlowCR1=0;

    /* Set {XHITH1, XHITH0, XLTH1, XLTH0} in FlowCR1 to {1, 0, 1, 1} depend on RD=64*/
    /* Turn on XNOEN in FlowCR1*/
    temp_FlowCR1 = (readb(ioaddr + FET_FlowCR1) | 0xB8);
    writeb(temp_FlowCR1, ioaddr + FET_FlowCR1);

    /* Set TxPauseTimer to 0xFFFF */
    writew(0xFFFF, ioaddr + FET_TxPauseTimer);

    /* Initialize RBRDU to Rx buffer count.*/
    writeb(64, ioaddr + FET_FlowCR0);
}

#ifdef netdev_timer
static void netdev_timer(unsigned long data)
{
    struct net_device *dev = (struct net_device *)data;
    netdev_private *np = (netdev_private *)dev->priv;
    int next_tick = 10*HZ;

#if (LINUX_VERSION_CODE >= 0x02032a)
    if (netif_queue_stopped(dev) != 0
        && np->cur_tx[0] - np->dirty_tx[0] > 1
        && jiffies - dev->trans_start > TX_TIMEOUT) {
#else
    if (test_bit(0, (void*)&dev->tbusy) != 0
        && np->cur_tx[0] - np->dirty_tx[0] > 1
        && jiffies - dev->trans_start > TX_TIMEOUT) {
#endif
        tx_timeout(dev);
        return;
    }
    // link change will generate interrupt so we must not set_media_duplex_mode in timer
    //set_media_duplex_mode(dev);
    

    np->timer.expires = jiffies + next_tick;
    add_timer(&np->timer);
}
#endif
#ifdef tx_timeout
static void tx_timeout(struct net_device *dev)
{
    netdev_private *np = (netdev_private *)dev->priv;
    unsigned long ioaddr = dev->base_addr;

    printk(KERN_INFO "%s: Transmit timed out, status %4.4x, PHY status "
           "%4.4x, resetting...\n",
           dev->name, readw(ioaddr + FET_ISR0),
           mdio_read(dev, np->phy_addr, 1));

    /* Perhaps we should reinitialize the hardware here. */
    dev->if_port = 0;
    /* Stop and restart the chip's Tx processes . */
    netdev_close(dev);
    netdev_open(dev);
        
    /* Trigger an immediate transmit demand. */
    dev->trans_start = jiffies;
    np->stats.tx_errors++;
    return;
}
#endif

/* Initialize the Rx and Tx rings, along with various 'dev' bits. */
static void init_ring(struct net_device *dev)
{
    netdev_private *np = (netdev_private *)dev->priv;
    int i, j;

    if (np->revision >= 0x80 && enable_tagging[np->card_idx] == 1) {
        for (i = 0; i < 8 ; i++) {
            np->tx_full[i] = 0;
            np->cur_tx[i] = 0;
            np->dirty_tx[i] = 0;
        }
    }
    else {
        np->tx_full[0] = 0;
        np->cur_tx[0] = 0;
        np->dirty_tx[0] = 0;
    }
    np->cur_rx = 0; 
    np->dirty_rx = 0;
    np->rx_buf_sz = (dev->mtu <= 1500 ? PKT_BUF_SZ : dev->mtu + 32);
    np->rx_head_desc = &np->rx_ring[0];

    for (i = 0; i < RX_RING_SIZE; i++) {
        np->rx_ring[i].rx_status = 0;
        np->rx_ring[i].desc_length = cpu_to_le32(np->rx_buf_sz);
        np->rx_ring[i].next_desc = virt_to_le32desc(&np->rx_ring[i+1]);
        np->rx_skbuff[i] = 0;
    }

    /* Mark the last entry as wrapping the ring. */
    np->rx_ring[i-1].next_desc = virt_to_le32desc(&np->rx_ring[0]);

    /* Fill in the Rx buffers.  Handle allocation failure gracefully. */
    for (i = 0; i < RX_RING_SIZE; i++) {
        struct sk_buff *skb = dev_alloc_skb(np->rx_buf_sz);

        np->rx_skbuff[i] = skb;
        if (skb == NULL)
            break;
        skb->dev = dev;         /* Mark as being used by this device. */
#if (LINUX_VERSION_CODE >= 0x020400)
        np->rx_skbuff_dma[i]=pci_map_single(np->pdev, skb->tail, np->rx_buf_sz, PCI_DMA_FROMDEVICE);

        np->rx_ring[i].addr = cpu_to_le32(np->rx_skbuff_dma[i]);
#else
        np->rx_ring[i].addr = virt_to_le32desc(skb->tail);
#endif
        np->rx_ring[i].rx_status = cpu_to_le32(FET_DescOwn);
    }
    np->dirty_rx = (unsigned int)(i - RX_RING_SIZE);

    if (np->revision >= 0x80 && enable_tagging[np->card_idx] == 1) {
        for (i = 0; i < 8; i++) {
#if (LINUX_VERSION_CODE >= 0x20341)
            np->tx_bufs[i] = pci_alloc_consistent(np->pdev, PKT_BUF_SZ * TX_RING_SIZE, &np->tx_bufs_dma[i]);
#endif
            for (j = 0; j < TX_RING_SIZE; j++) {
                np->tx_skbuff[i][j] = 0;
                np->tx_ring[i][j].tx_status = 0;
                np->tx_ring[i][j].desc_length = cpu_to_le32(0x00e08000);
                np->tx_ring[i][j].next_desc = virt_to_le32desc(&np->tx_ring[i][j+1]);
#if (LINUX_VERSION_CODE >= 0x20341)
                np->tx_buf[i][j] = np->tx_bufs[i] + PKT_BUF_SZ * j;
#else
                np->tx_buf[i][j] = kmalloc(PKT_BUF_SZ, GFP_KERNEL);
#endif
            }
            np->tx_ring[i][j-1].next_desc = virt_to_le32desc(&np->tx_ring[i][0]);
        }
    }
    else {
#if (LINUX_VERSION_CODE >= 0x20341)
        np->tx_bufs[0] = pci_alloc_consistent(np->pdev, PKT_BUF_SZ * TX_RING_SIZE, &np->tx_bufs_dma[0]);
#endif
        for (i = 0; i < TX_RING_SIZE; i++) {
            np->tx_skbuff[0][i] = 0;
            np->tx_ring[0][i].tx_status = 0;
            np->tx_ring[0][i].desc_length = cpu_to_le32(0x00e08000);
            np->tx_ring[0][i].next_desc = virt_to_le32desc(&np->tx_ring[0][i+1]);
#if (LINUX_VERSION_CODE >= 0x20341)
            np->tx_buf[0][i] = np->tx_bufs[0] + PKT_BUF_SZ * i;
#else
            np->tx_buf[0][i] = kmalloc(PKT_BUF_SZ, GFP_KERNEL);
//           printk("chenyp:np->tx_buf[0][ %d ]=%x\n",i, np->tx_buf[0][i]);

#endif
        }
        np->tx_ring[0][i-1].next_desc = virt_to_le32desc(&np->tx_ring[0][0]);
    }
    return;
}

static int start_tx(struct sk_buff *skb, struct net_device *dev)
{
    netdev_private *np = (netdev_private *)dev->priv;
    unsigned entry;
    unsigned int queue;
    unsigned int VID = ((VID_setting[np->card_idx] < 4095) && (VID_setting[np->card_idx] > 0))
                        ? VID_setting[np->card_idx] : 0;


//printk("chenyp:start_tx:phy0=%x, phy1=%x, phy2=%x, phy3=%x, phy4=%x, phy5=%x, mac8=%x, mac99=%x mac80=%x\n",mdio_read(dev,np->phy_addr,0),mdio_read(dev,np->phy_addr,1),mdio_read(dev,np->phy_addr,2),mdio_read(dev,np->phy_addr,3),mdio_read(dev,np->phy_addr,4),mdio_read(dev,np->phy_addr,5),readb(ioaddr+0x08),readb(ioaddr+0x99),readb(ioaddr+0x80));
    if( np->revision >= 0x80 && enable_tagging[np->card_idx] == 1)
        queue = skb->priority % 8;
    else
        queue = 0;

//printk("chenyp:start_tx:0x09=%x, 0x08=%x, tx=%x, rx=%x\n",readb(dev->base_addr+0x09), readb(dev->base_addr+0x08),readl(dev->base_addr+0x18),readl(dev->base_addr+0x1c));
    /* Block a timer-based transmit from overlapping.  This could better be
       done with atomic_swap(1, dev->tbusy), but set_bit() works as well. */

#if(LINUX_VERSION_CODE >= 0x02032a)
    if (netif_queue_stopped(dev)) {
        netif_stop_queue(dev);
        if (jiffies - dev->trans_start > TX_TIMEOUT)
              ;
//            tx_timeout(dev);
        return 1;
    }
    else
        netif_stop_queue(dev);
#else
    if (test_and_set_bit(0, (void*)&dev->tbusy) != 0) {
        /* This watchdog code is redundant with the media monitor timer. */
        if (jiffies - dev->trans_start > TX_TIMEOUT){
              ;
//            tx_timeout(dev);
        }
        return 1;
    }
#endif

    /* Explicitly flush packet data cache lines here. */

    /* Caution: the write order is important here, set the descriptor word
       with the "ownership" bit last.  No SMP locking is needed if the
       cur_tx is incremented after the descriptor is consistent.  */

    /* Calculate the next Tx descriptor entry. */
    entry = np->cur_tx[queue] % TX_RING_SIZE;
    np->tx_skbuff[queue][entry] = skb;
   
    if ((np->drv_flags & FET_ReqTxAlign)  && (long)skb->data & 3) {
		/* Must use alignment buffer. */
        memcpy(np->tx_buf[queue][entry], skb->data, skb->len);
	np->tx_ring[queue][entry].addr = virt_to_le32desc(np->tx_buf[queue][entry]);
    } else {
#if (LINUX_VERSION_CODE >= 0x020400)
        np->tx_skbuff_dma[queue][entry] = pci_map_single(np->pdev, skb->data, skb->len, PCI_DMA_TODEVICE);
//        printk("chenyp:start_tx:pci_map_single:tx_skbuff_dma[ %d ][ %d ]=%x, len=%x\n",queue,entry, np->tx_skbuff_dma[queue][entry],skb->len);

	np->tx_ring[queue][entry].addr =cpu_to_le32(np->tx_skbuff_dma[queue][entry]);
#else
	np->tx_ring[queue][entry].addr =virt_to_le32desc(skb->data);
#endif
    }
    np->tx_ring[queue][entry].desc_length =
        cpu_to_le32(0x00E08000 | (skb->len >= ETH_ZLEN ? skb->len : ETH_ZLEN));
    if (np->revision >= 0x80) {
        // for 802.1p/Q tagging
        // if enable tagging, set Instag = 1 in TCR of RD
        if (enable_tagging[np->card_idx] == 1 ) {
            np->tx_ring[queue][entry].desc_length |= cpu_to_le32(0x00020000);
            np->tx_ring[queue][entry].tx_status = cpu_to_le32(VID << 16);
            np->tx_ring[queue][entry].tx_status |= cpu_to_le32(queue << 28) ;
        }
        np->tx_ring[queue][entry].tx_status |= cpu_to_le32(FET_DescOwn);
    }
    else
        np->tx_ring[queue][entry].tx_status = cpu_to_le32(FET_DescOwn);    
    np->cur_tx[queue]++;

    /* Set the corresponding bits in TXQWAK to specify */
    /* packets in which queues are to be sent */
    if( np->revision >= 0x80 && enable_tagging[np->card_idx] == 1)
        writeb(1 << queue, dev->base_addr + FET_TXQWAK);

    /* Explicitly flush descriptor cache lines here. */

    /* Wake the potentially-idle transmit channel. */
    writeb(FET_CmdTxDemand1 | readb(dev->base_addr + FET_CR1) , dev->base_addr + FET_CR1);
    if (np->cur_tx[queue] - np->dirty_tx[queue] < TX_QUEUE_LEN - 1)
#if (LINUX_VERSION_CODE >= 0x02032a)
        netif_wake_queue(dev);
#else
        clear_bit(0, (void*)&dev->tbusy);       /* Typical path */
#endif
    else
    
        np->tx_full[queue] = 1;
    dev->trans_start = jiffies;
    if (debug > 4) {
        printk(KERN_INFO "%s: Transmit frame #%d queued in slot %d.\n",
               dev->name, np->cur_tx[queue], entry);
    }
//    printk("chenyp:intr:%x %x \n",readb(ioaddr+0x08),readb(ioaddr+0x09));
    return 0;
}



static void netdev_tx(struct net_device *dev)
{
    netdev_private *np = (netdev_private *)dev->priv;
    unsigned long ioaddr = dev->base_addr;
    unsigned int queue;

    for (queue = 0; queue < 8; queue++) {
        for (; np->cur_tx[queue] - np->dirty_tx[queue] > 0; np->dirty_tx[queue]++) {
            int entry = np->dirty_tx[queue] % TX_RING_SIZE;
            int txstatus = le32_to_cpu(np->tx_ring[queue][entry].tx_status);

            if (txstatus & FET_DescOwn)
                break;
            if (debug > 6)
                printk(KERN_INFO " Tx scavenge %d status %4.4x.\n",
                       entry, txstatus);
            if (txstatus & 0x8000) {
                if (debug > 1)
                    printk(KERN_INFO "%s: Transmit error, Tx status %4.4x.\n",
                           dev->name, txstatus);
                np->stats.tx_errors++;
                
                if (txstatus & 0x0800) { 
                    /* uderrun happen */
                    if (np->tx_thresh < 0x04) {
                        np->tx_thresh += 1;
                        writeb((readb(ioaddr + FET_BCR1) & 0xc7) | (np->tx_thresh << 3), ioaddr + FET_BCR1);
                        writeb((readb(ioaddr + FET_TCR) & 0x1f) | (np->tx_thresh << 5), ioaddr + FET_TCR);
                    }
                    if (debug > 1)
                        printk(KERN_INFO "%s: Transmitter underrun, increasing Tx "
                        "threshold setting to %2.2x.\n", dev->name, np->tx_thresh);

                    np->tx_ring[queue][entry].tx_status = cpu_to_le32(FET_DescOwn);
                    writel(virt_to_bus(&np->tx_ring[queue][entry]), ioaddr + FET_CURR_TX_DESC_ADDR + (queue * 4));
                    if (np->revision >= 0x80 && enable_tagging[np->card_idx] == 1)
                        /* Set the corresponding bits in TXQWAK to specify */
                        /* packets in which queues are to be sent */
                        writeb(1 << queue, dev->base_addr + FET_TXQWAK);

                    /* Turn on Tx On*/
                    writew(FET_CmdTxOn | np->chip_cmd, dev->base_addr + FET_CR0);        

                    /* Stats counted in Tx-done handler, just restart Tx. */
                    writeb(FET_CmdTxDemand1 | readb(dev->base_addr + FET_CR1) , dev->base_addr + FET_CR1);

                    break;
                }    
                if (txstatus & 0x0400) np->stats.tx_carrier_errors++;
                if (txstatus & 0x0200) np->stats.tx_window_errors++;
                if (txstatus & 0x0100) { 
                    np->stats.tx_aborted_errors++;
                    np->tx_ring[queue][entry].tx_status = cpu_to_le32(FET_DescOwn);
                    writel(virt_to_bus(&np->tx_ring[queue][entry]), ioaddr + FET_CURR_TX_DESC_ADDR + (queue * 4));

                    /* Set the corresponding bits in TXQWAK to specify */
                    /* packets in which queues are to be sent */
                    if (np->revision >= 0x80 && enable_tagging[np->card_idx] == 1)
                        writeb(1 << queue, dev->base_addr + FET_TXQWAK);

                    /* Turn on Tx On*/
                    writew(FET_CmdTxOn | np->chip_cmd, dev->base_addr + FET_CR0);        
                    /* Stats counted in Tx-done handler, just restart Tx. */
                    writeb(FET_CmdTxDemand1 | readb(dev->base_addr + FET_CR1) , dev->base_addr + FET_CR1);
                    printk(KERN_ERR "Tx Abort");
                    break; 
                }    
                if (txstatus & 0x0080) np->stats.tx_heartbeat_errors++;
                if (txstatus & 0x0002) np->stats.tx_fifo_errors++;
#ifdef ETHER_STATS
                if (txstatus & 0x0100) np->stats.collisions16++;
#endif
                /* Transmitter restarted in 'abnormal' handler. */
            } else {
#ifdef ETHER_STATS
                if (txstatus & 0x0001) np->stats.tx_deferred++;
#endif
                np->stats.collisions += (txstatus >> 3) & 15;
#if defined(NETSTATS_VER2)
                np->stats.tx_bytes += np->tx_skbuff[queue][entry]->len;
#endif
                np->stats.tx_packets++;
            }
            /* Free the original skb. */
#if (LINUX_VERSION_CODE >= 0x020400)
            if (np->tx_skbuff_dma[queue][entry] != 0) {
                pci_unmap_single(np->pdev, np->tx_skbuff_dma[queue][entry], np->tx_skbuff[queue][entry]->len, PCI_DMA_TODEVICE);
//                printk("chenyp:netdev_tx:pci_unmap_single: np->tx_skbuff_dma[ %d ][ %d ]=%x,len=%x\n",queue,entry, np->tx_skbuff_dma[queue][entry],np->tx_skbuff[queue][entry]->len);
                np->tx_skbuff_dma[queue][entry] = 0;
            }
#endif
            dev_free_skb(np->tx_skbuff[queue][entry]);

            np->tx_skbuff[queue][entry] = 0;
        }
#if (LINUX_VERSION_CODE >= 0x02032a)
        if (np->tx_full[queue] && netif_queue_stopped(dev)
            && np->cur_tx[queue] - np->dirty_tx[queue] < TX_QUEUE_LEN - 4) {
#else
        if (np->tx_full[queue] && dev->tbusy
            && np->cur_tx[queue] - np->dirty_tx[queue] < TX_QUEUE_LEN - 4) {
#endif
            /* The ring is no longer full, clear tbusy. */
            np->tx_full[queue] = 0;
#if (LINUX_VERSION_CODE >= 0x02032a)
            netif_wake_queue(dev);
#else
            clear_bit(0, (void*)&dev->tbusy);
            mark_bh(NET_BH);
#endif
        }
        if (np->revision < 0x80 || (np->revision >= 0x80 && enable_tagging[np->card_idx] != 1))
            break;
    }
}

/* The interrupt handler does all of the Rx thread work and cleans up
   after the Tx thread. */
static void intr_handler(int irq, void *dev_instance, struct pt_regs *rgs)
{
    struct net_device *dev = (struct net_device *)dev_instance;
    unsigned long ioaddr = dev->base_addr;
    int boguscnt = max_interrupt_work;


    do {
        u32 intr_status = readw(ioaddr + FET_ISR0);

        /* Acknowledge all of the current interrupt sources ASAP. */
        writew(intr_status & 0xffff, ioaddr + FET_ISR0);

        if (debug > 4)
            printk(KERN_INFO "%s: Interrupt, status %4.4x.\n",
                   dev->name, intr_status);

        if (intr_status == 0)
            break;
//printk("chenyp:intr_hand:phy0=%x, phy1=%x, phy2=%x, phy3=%x, phy4=%x, phy5=%x, mac8=%x, mac9=%x mac6d=%x\n",mdio_read(dev,np->phy_addr,0),mdio_read(dev,np->phy_addr,1),mdio_read(dev,np->phy_addr,2),mdio_read(dev,np->phy_addr,3),mdio_read(dev,np->phy_addr,4),mdio_read(dev,np->phy_addr,5),readb(ioaddr+0x08),readb(ioaddr+0x09),readb(ioaddr+0x6d));

        if (intr_status & (FET_ISR_PRX | FET_ISR_RXE | FET_ISR_PKTRACE |
                           FET_ISR_GENI | FET_ISR_RU | FET_ISR_NORBF))
            netdev_rx(dev);
	netdev_tx(dev);
//chenyp
//    printk("chenyp:intr:%x %x \n",readb(ioaddr+0x08),readb(ioaddr+0x09));
        /* Abnormal error summary/uncommon events handlers. */
        if (intr_status & (FET_ISR_BE | FET_ISR_SRCI | FET_ISR_CNT | FET_ISR_TXE | FET_ISR_TU))
            netdev_error(dev, intr_status);

        if (--boguscnt < 0) {
            if (debug > 1) 
                printk(KERN_INFO "%s: Too much work at interrupt, "
                       "status=0x%4.4x.\n",
                   dev->name, intr_status);
            break;
        }
    } while (1);

    if (debug > 3)
        printk(KERN_INFO "%s: exiting interrupt, status=%#4.4x.\n",
               dev->name, readw(ioaddr + FET_ISR0));

    return;
}

static void checksum_offload(struct sk_buff *skb, u32 rx_PQSTS)
{
    if (rx_PQSTS & IPKT) {
        if (!(rx_PQSTS & IPOK)) {
            skb->ip_summed = CHECKSUM_NONE;
            return;
        }
        else {
            if (rx_PQSTS & TCPKT) {
                if (!(rx_PQSTS & TUOK)) {
                    skb->ip_summed = CHECKSUM_NONE;
                    return;
                }
            }
            if (rx_PQSTS & UDPKT) {
                if (!(rx_PQSTS & TUOK)) {
                    skb->ip_summed = CHECKSUM_NONE;
                    return;
                }
            }
        }
        skb->ip_summed = CHECKSUM_UNNECESSARY;
    }
}
/* Because our RD needed be DWORD byte aligned, so IP won't be DWORD byte aligned*/
/* We must shift 2 bytes of the data for IP header DWORD byte aligned */
static void shift_for_IP_byte_aligned(struct sk_buff *skb, unsigned int pkt_len)
{
//   char tmp;
    int i;

    for (i = pkt_len - 1; i >= 0 ; i--)
        *(skb->data + i + 2) = *(skb->data + i);
    skb->data += 2;
    skb->tail += 2;
}

/* This routine is logically part of the interrupt handler, but isolated
   for clarity and better register allocation. */
static int netdev_rx(struct net_device *dev)
{
    netdev_private *np = (netdev_private *)dev->priv;
    unsigned long ioaddr = dev->base_addr;
    int entry = np->cur_rx % RX_RING_SIZE;
    int boguscnt = np->dirty_rx + RX_RING_SIZE - np->cur_rx;
    u32 rx_PQSTS;

    if (debug > 4) {
        printk(KERN_INFO " In netdev_rx(), entry %d status %8.8x.\n",
               entry, np->rx_head_desc->rx_status);
    }

    /* If EOP is set on the next entry, it's a new packet. Send it up. */
    while ( ! (np->rx_head_desc->rx_status & cpu_to_le32(FET_DescOwn))) {
        rx_desc *desc = np->rx_head_desc;
        u32 desc_status = le32_to_cpu(desc->rx_status);
        int data_size = desc_status >> 16;

        if (debug > 4)
            printk(KERN_INFO "  netdev_rx() status is %4.4x.\n",
                   desc_status);
        if (--boguscnt < 0)
            break;

#if (LINUX_VERSION_CODE >= 0x020400)
        if (np->rx_skbuff_dma[entry] != 0) {
            pci_unmap_single (np->pdev, np->rx_skbuff_dma[entry], np->rx_buf_sz , PCI_DMA_FROMDEVICE);
//               printk("chenyp:netdev_rx:pci_unmap_single: rx_skbuff_dma[ %d ]=%x, len=%x\n",entry,np->rx_skbuff_dma[entry],np->rx_buf_sz);
            np->rx_skbuff_dma[entry] = 0;
        }
#endif
        /* for 802.1p/Q tagging, if enable tagging,and we don't set VID (VID = 0) */
        /* then we must drops untagged packets */
        if (np->revision >= 0x80 && enable_tagging[np->card_idx] == 1 && VID_setting[np->card_idx] != 0) {
            /* untagged frame ,drop it*/
            if (!(np->rx_head_desc->desc_length & 0x00010000)) {
                entry = (++np->cur_rx) % RX_RING_SIZE;
                np->rx_head_desc = &np->rx_ring[entry];
                continue;
            }
        }
        if ( (desc_status & (FET_RxWholePkt | FET_RxErr)) !=  FET_RxWholePkt) {
            if ((desc_status & FET_RxWholePkt) !=  FET_RxWholePkt) {
                printk(KERN_INFO "%s: Oversized Ethernet frame spanned "
                       "multiple buffers, entry %#x length %d status %4.4x!\n",
                       dev->name, np->cur_rx, data_size, desc_status);
                printk(KERN_INFO "%s: Oversized Ethernet frame %p vs %p.\n",
                       dev->name, np->rx_head_desc,
                       &np->rx_ring[np->cur_rx % RX_RING_SIZE]);
                np->stats.rx_length_errors++;
            } else if (desc_status & FET_RxErr) {
                /* There was a error. */
                if (debug > 2)
                    printk(KERN_INFO "  netdev_rx() Rx error was %8.8x.\n",
                           desc_status);
                np->stats.rx_errors++;
                if (desc_status & 0x0030) np->stats.rx_length_errors++;
                if (desc_status & 0x0048) np->stats.rx_fifo_errors++;
                if (desc_status & 0x0004) np->stats.rx_frame_errors++;
                if (desc_status & 0x0002) np->stats.rx_crc_errors++;
            }
        } else {
            struct sk_buff *skb;
            /* Length should omit the CRC */
            int pkt_len = data_size - 4;
            unsigned int wLen = 0, wSAP = 0, wActualLen = 0;

            /* For conforming IEEE 802.3 spec
             * If the incoming packet is IEE 802.3 frmae/IEEE 802.3 SNAP frame, get
             * RX_Length in RDES0 from the incoming packet, subtract Ethernet header
             * length and CRC length from it. Then, compare the result with L/T field
             * of the packet. If they're not equal, descard this packet.
             */

            wLen = (*(np->rx_skbuff[entry]->tail + 12) << 8) + (*(np->rx_skbuff[entry]->tail + 13));
            if ( wLen >= 46 && wLen <= 1500) {   // IEEE 802.3/IEEE 802.3 SNAP frame
    	        wSAP = (*(np->rx_skbuff[entry]->tail + 14) << 8) + (*(np->rx_skbuff[entry]->tail + 15));
                if (wSAP != 0xFFFF)               // exclude Novell's Ethernet 802.3 frame
                    wActualLen = data_size - U_HEADER_LEN - U_CRC_LEN;         // real packet length
            }
            if (!(wLen >= 46 && wLen <= 1500 && wSAP != 0xFFFF && wLen != wActualLen)) {
                /* Check if the packet is long enough to accept without copying
                   to a minimally-sized skbuff. */
                if (pkt_len < rx_copybreak
                    && (skb = dev_alloc_skb(pkt_len + 2)) != NULL) {
//        printk("chenyp:dev_alloc_skb:skb=%x\n",skb);

                    skb->dev = dev;
                    skb_reserve(skb, 2);    /* 16 byte align the IP header */
#if HAS_IP_COPYSUM          /* Call copy + cksum if available. */
                    eth_copy_and_sum(skb, np->rx_skbuff[entry]->tail, pkt_len, 0);
                    skb_put(skb, pkt_len);
#else
                    memcpy(skb_put(skb, pkt_len), np->rx_skbuff[entry]->tail,
                           pkt_len);
#endif
        	} else {
        	    if( IP_byte_align[np->card_idx] == 1) { /* for byte align the IP header, or the checksum will fail in some condition*/
                        skb = np->rx_skbuff[entry];
                        shift_for_IP_byte_aligned(skb, pkt_len);
                        skb_put(skb, pkt_len);
                        np->rx_skbuff[entry] = NULL;
                    }
                    else {
                        skb_put(skb = np->rx_skbuff[entry], pkt_len);
                        np->rx_skbuff[entry] = NULL;
                    }
                }
                skb->protocol = eth_type_trans(skb, dev);
                skb->ip_summed = CHECKSUM_NONE;

                /* receive checksum_offload */
                if (np->revision >= 0x80 && csum_offload[np->card_idx] == 1) {
                    rx_PQSTS = le32_to_cpu((np->rx_head_desc->desc_length << 8) >> 24);
                    checksum_offload(skb, rx_PQSTS);
                }
                netif_rx(skb);
                dev->last_rx = jiffies;
#if defined(NETSTATS_VER2)
                np->stats.rx_bytes += skb->len;
#endif
                np->stats.rx_packets++;
            }
        }
        entry = (++np->cur_rx) % RX_RING_SIZE;
        np->rx_head_desc = &np->rx_ring[entry];
    }

    /* Refill the Rx ring buffers. */
    for (; np->cur_rx - np->dirty_rx > 0; np->dirty_rx++) {
        struct sk_buff *skb;
        entry = np->dirty_rx % RX_RING_SIZE;
        if (np->rx_skbuff[entry] == NULL) {
            skb = dev_alloc_skb(np->rx_buf_sz);
//        printk("chenyp:dev_alloc_skb:skb=%x\n",skb);

            np->rx_skbuff[entry] = skb;
            if (skb == NULL)
                break;          /* Better luck next round. */
            skb->dev = dev;         /* Mark as being used by this device. */
#if (LINUX_VERSION_CODE >= 0x020400)
            np->rx_skbuff_dma[entry]=pci_map_single(np->pdev, skb->tail, np->rx_buf_sz, PCI_DMA_FROMDEVICE);
//            printk("chenyp:netdev_rx:pci_map_single:rx_skbuff_dma%d=%x, len=%x\n",entry, np->rx_skbuff_dma[entry],np->rx_buf_sz);

            np->rx_ring[entry].addr = cpu_to_le32(np->rx_skbuff_dma[entry]);
#else
            np->rx_ring[entry].addr = virt_to_le32desc(skb->tail);
#endif
        }
        np->rx_ring[entry].rx_status = cpu_to_le32(FET_DescOwn);

        // update RXRDU in FlowCR1 after return descriptor/buffer to MAC.
        if (np->revision >=0x80 )
            writeb(1, ioaddr + FET_FlowCR0);
    }

    /* Pre-emptively restart Rx engine. */
    //writew(FET_CmdRxDemand | np->chip_cmd, dev->base_addr + FET_CR0);
    return 0;
}

static void netdev_error(struct net_device *dev, int intr_status)
{
    netdev_private *np = (netdev_private *)dev->priv;
    unsigned long ioaddr = dev->base_addr;
    int i;

    /* Port Status change*/
    if (intr_status & FET_ISR_SRCI ) {
        /*check whether link fail */
        if (readb(ioaddr + FET_MIISR) & 0x02) {
            printk(KERN_INFO "%s: Link Fail.\n",dev->name);
            np->full_duplex = 0;
            np->chip_cmd &= FET_CmdNoFDuplex;
	} else {
            if (np->revision >=0x80)
                set_flow_control(dev);

            if (np->revision < 0x20 )
                /* wait for writing  back the ANLPAR in PHY and MIISR in MAC */
                /* then we won't get the wrong value in do_autonegotiation function */
                mdelay(100);

            /* In auto mode, check the speed and duplex mode again */
            /* In force mode, do nothing */
            if (np->auto_negotiation == 1)
                do_autonegotiation(dev);
            printk(KERN_INFO "%s: Link success.\n", dev->name);

            /* if VT3106 and VT3065 */
            if (np->revision >= 0x40)
                flow_control_ability (dev);
        }
        if (debug > 5)
            printk(KERN_ERR "%s: MII status changed: Autonegotiation "
                   "advertising %4.4x  partner %4.4x.\n", dev->name,
               mdio_read(dev, np->phy_addr, 4),
               mdio_read(dev, np->phy_addr, 5)
               );            
    }
    if (intr_status & FET_ISR_CNT) {
        np->stats.rx_crc_errors += readw(ioaddr + FET_CRC_tally);
        np->stats.rx_missed_errors  += readw(ioaddr + FET_MPA_tally);
        writel(0, ioaddr + FET_MPA_tally);
    }
    if (intr_status & FET_ISR_TXE) {
             
       
    }
    if (intr_status & FET_ISR_TU) {
    }
    if ((intr_status & ~(FET_ISR_SRCI | FET_ISR_CNT |
                         FET_ISR_TXE|FET_ISR_ABTI))) {
        if (debug > 3)                                                
        printk(KERN_ERR "%s: Something Wicked happened! %4.4x.\n",
               dev->name, intr_status);
        if (np->revision >= 0x80)
            for(i = 0; i < 8; i++) {
                /* Set the corresponding bits in TXQWAK to specify */
                /* packets in which queues are to be sent */
                writeb(1 << i, dev->base_addr + FET_TXQWAK);
            }
        /* Recovery for other fault sources not known. */
        writeb(FET_CmdTxDemand1 | readb(dev->base_addr + FET_CR1) , dev->base_addr + FET_CR1);
    }
       
}

static struct net_device_stats *get_stats(struct net_device *dev)
{
    netdev_private *np = (netdev_private *)dev->priv;
    unsigned long ioaddr = dev->base_addr;

    /* Nominally we should lock this segment of code for SMP, although
       the vulnerability window is very small and statistics are
       non-critical. */
    np->stats.rx_crc_errors += readw(ioaddr + FET_CRC_tally);
    np->stats.rx_missed_errors  += readw(ioaddr + FET_MPA_tally);
    writel(0, ioaddr + FET_MPA_tally);
    np->stats.rx_missed_errors=0;
    np->stats.rx_crc_errors=0;
    return &np->stats;
}

/* The big-endian AUTODIN II ethernet CRC calculation.
   N.B. Do not use for bulk data, use a table-based routine instead.
   This is common code and should be moved to net/core/crc.c */
static unsigned const ethernet_polynomial = 0x04c11db7U;
static inline u32 ether_crc(int length, unsigned char *data)
{
    int crc = -1;

    while(--length >= 0) {
        unsigned char current_octet = *data++;
        int bit;
        for (bit = 0; bit < 8; bit++, current_octet >>= 1) {
            crc = (crc << 1) ^
                ((crc < 0) ^ (current_octet & 1) ? ethernet_polynomial : 0);
        }
    }
    return crc;
}

static void set_rx_mode(struct net_device *dev)
{
    unsigned long ioaddr = dev->base_addr;
    u32 mc_filter[2];           /* Multicast hash filter */
    u8 rx_mode;                 /* Note: 0x02=accept runt, 0x01=accept errs */

    if (dev->flags & IFF_PROMISC) {         /* Set promiscuous. */
        /* Unconditionally log net taps. */
        printk(KERN_NOTICE "%s: Promiscuous mode enabled.\n", dev->name);
        rx_mode = 0x1C;
    } else if ((dev->mc_count > multicast_filter_limit)
               ||  (dev->flags & IFF_ALLMULTI)) {
        /* Too many to match, or accept all multicasts. */
        writel(0xffffffff, ioaddr + FET_MAR0);
        writel(0xffffffff, ioaddr + FET_MAR4);
        rx_mode = 0x0C;
    } else {
        struct dev_mc_list *mclist;
        int i;
        memset(mc_filter, 0, sizeof(mc_filter));
        for (i = 0, mclist = dev->mc_list; mclist && i < dev->mc_count;
             i++, mclist = mclist->next) {
            set_bit(ether_crc(ETH_ALEN, mclist->dmi_addr) >> 26,
                    mc_filter);
        }
        writel(mc_filter[0], ioaddr + FET_MAR0);
        writel(mc_filter[1], ioaddr + FET_MAR4);
        rx_mode = 0x0C;
    }
    writeb(readb(ioaddr + FET_RCR) | rx_mode, ioaddr + FET_RCR);
}

static int mii_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	u16 *data = (u16 *)&rq->ifr_data;

	switch(cmd) {
	case SIOCGMIIPHY:		/* Get the address of the PHY in use. */
		data[0] = ((netdev_private *)dev->priv)->phy_addr & 0x1f;
		/* Fall Through */
	case SIOCGMIIREG:		/* Read the specified MII register. */
		data[3] = mdio_read(dev, data[0] & 0x1f, data[1] & 0x1f);
		return 0;
	case SIOCSMIIREG:		/* Write the specified MII register */
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		mdio_write(dev, data[0] & 0x1f, data[1] & 0x1f, data[2]);
		return 0;
	default:
		return -EOPNOTSUPP;
	}
}

static int netdev_close(struct net_device *dev)
{
    unsigned long ioaddr = dev->base_addr;
    netdev_private *np = (netdev_private *)dev->priv;
    int i, j;
    int ww;

#if (LINUX_VERSION_CODE >= 0x02032a)
    netif_stop_queue(dev);
#else
    dev->start = 0;
    dev->tbusy = 1;
#endif
    if (debug > 1)
        printk(KERN_INFO "%s: Shutting down ethercard, status was %4.4x.\n",
               dev->name, readw(ioaddr + FET_CR0));

    /* Disable interrupts by clearing the interrupt mask. */
    writew(0x0000, ioaddr + FET_IMR0);
    /* if the chip is 3065, we must patch shutdown bug*/
    
    if ((np->revision >= 0x40) && (np->revision <= 0x80)) { 

        //Nic Loop Back On
        writeb(readb(ioaddr + FET_TCR) | 0x01, ioaddr + FET_TCR);
        
       //Tx Off
        writeb(readb(ioaddr + FET_CR0) & 0xEF, ioaddr + FET_CR0);
        
        // W_MAX_TIMEOUT is the timeout period
        for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
            if((readb(ioaddr + FET_CR0) & 0x10) == 0)
                break; 
        } 
        
        //Rx Off
        writeb(readb(ioaddr + FET_CR0) & 0xF7, ioaddr + FET_CR0);
        
        // W_MAX_TIMEOUT is the timeout period
        for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
            if((readb(ioaddr + FET_CR0) & 0x08) == 0)
                break; 
        } 
        
        if (ww == W_MAX_TIMEOUT) {
    
            // Turn on fifo test
            writew(readw(ioaddr + FET_GFTEST) | 0x0001, ioaddr + FET_GFTEST);
            // Turn on fifo reject
            writew(readw(ioaddr + FET_GFTEST) | 0x0800, ioaddr + FET_GFTEST);
            // Turn off fifo test
            writew(readw(ioaddr + FET_GFTEST) & 0xFFFE, ioaddr + FET_GFTEST);
        }
        
        //Nic Loop Back Off
        writeb(readb(ioaddr + FET_TCR) & 0xFE, ioaddr + FET_TCR);
    }
    
    /* Stop the chip's Tx and Rx processes. */
    writew(FET_CmdStop, ioaddr + FET_CR0);
    del_timer(&np->timer);

    free_irq(dev->irq, dev);
    /* Free all the skbuffs in the Rx queue. */
    for (i = 0; i < RX_RING_SIZE; i++) {
        np->rx_ring[i].rx_status = 0;
        np->rx_ring[i].addr = 0xBADF00D0; /* An invalid address. */
        if (np->rx_skbuff[i]) {
#if LINUX_VERSION_CODE < 0x20100
            np->rx_skbuff[i]->free = 1;
#endif
            dev_free_skb(np->rx_skbuff[i]);
//            printk("chenyp:dev_free_skb: np->rx_skbuff[ %d ]=%x\n",i,np->rx_skbuff[i]);
        }
        np->rx_skbuff[i] = 0;
    }
    if (np->revision >= 0x80) {
        for (j = 0; j < 8; j++) {
#if (LINUX_VERSION_CODE >= 0x20341)
            if (np->tx_bufs[j])
                pci_free_consistent(np->pdev, PKT_BUF_SZ * TX_RING_SIZE, np->tx_bufs[j], np->tx_bufs_dma[j]);
#endif
            for (i = 0; i < TX_RING_SIZE; i++) {
                if (np->tx_skbuff[j][i])
                    dev_free_skb(np->tx_skbuff[j][i]);
//                    printk("chenyp:dev_free_skb:np->tx_skbuff[ %d ][ %d ]=%x\n",j, i,np->tx_skbuff[j][i]);

                np->tx_skbuff[j][i] = 0;
                if (np->tx_buf[j][i]) {
#if (LINUX_VERSION_CODE < 0x20341)
                    kfree(np->tx_buf[j][i]);
//                    printk("chenyp:kfree:np->tx_buf[ %d ][ %d ]=%x\n",j,i,np->tx_buf[j][i]);
#endif
                    np->tx_buf[j][i] = 0;
                }
            }
        }
    }
    else {
#if (LINUX_VERSION_CODE >= 0x20341)
        if (np->tx_bufs[0]) {
            pci_free_consistent(np->pdev, PKT_BUF_SZ * TX_RING_SIZE, np->tx_bufs[0], np->tx_bufs_dma[0]);
        }

#endif
        for (i = 0; i < TX_RING_SIZE; i++) {
            if (np->tx_skbuff[0][i])
                dev_free_skb(np->tx_skbuff[0][i]);
//                    printk("chenyp:dev_free_skb:tx_skbuff[0][ %d ]=%x\n",i,np->tx_skbuff[0][i]);

            np->tx_skbuff[0][i] = 0;
#if (LINUX_VERSION_CODE < 0x20341)
            if (np->tx_buf[0][i]) {
                kfree(np->tx_buf[0][i]);
//                    printk("chenyp:kfree:np->tx_buf[ 0 ][ %d ]=%x\n",i,np->tx_buf[0][i]);

                np->tx_buf[0][i] = 0;
            }
#endif
        }
    }
    MOD_DEC_USE_COUNT;
    return 0;
}

#ifndef MODULE
#if LINUX_VERSION_CODE > 0x02032a


enum via_rhine_chips {
         VT3043 = 0,
         VT6102,
         VT6105,
};

static struct pci_device_id linux_fet_pci_tbl[] __devinitdata =
{
	{0x1106, 0x3043, PCI_ANY_ID, PCI_ANY_ID, 0, 0, VT3043},
	{0x1106, 0x3065, PCI_ANY_ID, PCI_ANY_ID, 0, 0, VT6102},
	{0x1106, 0x3106, PCI_ANY_ID, PCI_ANY_ID, 0, 0, VT6105},
	{0,}			/* terminate list */
};

MODULE_DEVICE_TABLE(pci, linux_fet_pci_tbl);
static int __devinit linux_fet_init_one(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    struct net_device *dev;
    netdev_private *np;
    int i;
    unsigned char byOrgValue;
    int ww;
    u8 mode3_reg;
    u32 revision=0x42;
    int chip_idx = (int) ent->driver_data;
    static int card_idx = -1;
    long ioaddr;
    int io_size;
    int pci_flags;
    u32 pci_class_rev;
    pci_read_config_dword(pdev, PCI_REVISION_ID, &pci_class_rev);

    revision = (pci_class_rev << 24) >> 24;
    card_idx++;
	pci_flags = pci_tbl[chip_idx].pci_flags;
	io_size = pci_tbl[chip_idx].io_size;

        /* Ask low-level code to enable I/O and memory. */
        /* Wake up the device if it was suspended. */
	if (pci_enable_device (pdev))
		goto err_out;

	/* this should always be supported */
	if (!pci_dma_supported(pdev, 0xffffffff)) {
		printk(KERN_ERR "32-bit PCI DMA addresses not supported by the card!?\n");
		goto err_out;
	}
	pdev->dma_mask=0xffffffff;
	/* sanity check */
	if ((pci_resource_len (pdev, 0) < io_size) ||
	    (pci_resource_len (pdev, 1) < io_size)) {
		printk (KERN_ERR "Insufficient PCI resources, aborting\n");
		goto err_out;
	}

	ioaddr = pci_resource_start (pdev,  0);
	/* enable PCI bus-mastering */
//	if (pci_flags & PCI_USES_MASTER)
		pci_set_master (pdev);
	/* request all PIO and MMIO regions just to make sure
	 * noone else attempts to use any portion of our I/O space */



	dev = alloc_etherdev(sizeof(*np));
	if (dev == NULL) {
		printk (KERN_ERR "init_ethernet failed for card #%d\n",
			card_idx);
		goto err_out_free_dma;
	}
	SET_MODULE_OWNER(dev);
	if (register_netdev (dev)){
            
	    goto err_out;
        }
	if (pci_request_regions (pdev, dev->name))
		goto err_out;

    // if vt3065     
    if (revision>=0x40) {
       // clear sticky bit before reset & read ethernet address
       byOrgValue = readb(ioaddr + FET_STICKHW);    
       byOrgValue = byOrgValue & 0xFC;
       writeb(byOrgValue, ioaddr + FET_STICKHW);        
       // disable force PME-enable 
       writeb(0x80, ioaddr + FET_WOLCG_CLR);
       // disable power-event config bit
       writeb(0xFF, ioaddr + FET_WOLCR_CLR);
       // clear power status 
       writeb(0xFF, ioaddr + FET_PWRCSR_CLR);   
    }
    
        
    /* Reset the chip to erase previous misconfiguration. */
    writew(FET_CmdReset, ioaddr + FET_CR0);
    // if vt3043 delay after reset
    if (revision <0x40) {
       udelay(10000);
    }

    // polling till software reset complete
    // W_MAX_TIMEOUT is the timeout period
    for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
        if ((readw(ioaddr + FET_CR0) & FET_CmdReset) == 0 )
            break;
    }
       
    // issue AUTOLoad in EECSR to reload eeprom
    writeb(0x20, ioaddr + FET_EECSR);
      
    // if vt3065 delay after reset
    if (revision >=0x40 ) { 

        // delay 8ms to let MAC stable
        mdelay(8);
        // for 3065D, EEPROM reloaded will cause bit 0 in CFGA turned on.
        // it makes MAC receive magic packet automatically. So, driver turn it off.
        writeb(readb(ioaddr + FET_CFGA) & 0xFE, ioaddr + FET_CFGA);
    }

    /* turn on bit2 in PCI configuration register 0x53 , only for 3065*/
    if (revision >= 0x40) {
        pci_read_config_byte(pdev,PCI_REG_MODE3,&mode3_reg);
        pci_write_config_byte(pdev,PCI_REG_MODE3,mode3_reg|MODE3_MIION);
    }

    /* back off algorithm ,disable the right-most 4-bit off FET_CFGD*/
    writeb(readb(ioaddr + FET_CFGD) & (~(FET_CRADOM | FET_CAP | FET_MBA | FET_BAKOPT)), ioaddr + FET_CFGD);

 //   dev = init_etherdev(init_dev, 0);

    printk(KERN_INFO "%s: %s\n",dev->name, pci_tbl[chip_idx].name);
    printk(KERN_INFO "%s: IO Address = 0x%lx, MAC Address = ",dev->name, ioaddr);
    /* Ideally we would read the EEPROM but access may be locked. */
    for (i = 0; i < 6; i++)
        dev->dev_addr[i] = readb(ioaddr + FET_PAR + i);
    for (i = 0; i < 5; i++)
            printk("%2.2x:", dev->dev_addr[i]);
    printk("%2.2x, IRQ = %d.\n", dev->dev_addr[i], pdev->irq);
    if (init_device_data (dev, pdev, ioaddr, revision, pdev->irq, chip_idx, card_idx) == -1)
       return -ENOMEM;
    return 0;
err_out_free_dma:
err_out:
	return -ENODEV;
}
static void __devexit linux_fet_remove_one (struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata (pdev);
//	struct rtl8139_private *np;

	printk ("%s: enter linux_fet_remove_one\n", dev->name);

//	assert (dev != NULL);
//	np = dev->priv;
//	assert (np != NULL);

//	unregister_netdev (dev);

//	__rtl8139_cleanup_dev (dev);

	printk ("%s: leave linux_fet_remove_one\n", dev->name);

}

static struct pci_driver linux_fet_driver = {
	name:		"linux_fet",
	id_table:	linux_fet_pci_tbl,
	probe:		linux_fet_init_one,
	remove:		linux_fet_remove_one,

};

static int __init linux_fet_init (void)
{
/* this is printed whether or not devices are found in probe */

	printk(version);
/* ifdef MODULE */
	return pci_module_init (&linux_fet_driver);
}

static void __exit linux_fet_cleanup (void)
{
//	printk("chenyp:exit binding\n");

	pci_unregister_driver (&linux_fet_driver);
}

module_init(linux_fet_init);
module_exit(linux_fet_cleanup);

#else /* LINUX_VERSION_CODE > 0x02032a */
int via_rhine_probe(struct net_device *dev)
{
    if (debug)
        printk(KERN_INFO "%s", version);
    return pci_drv_register(&via_rhine_drv_id, dev);
}
#endif /* if LINUX_VERSION_CODE > 0x02032a */
#endif /* ifndef MODULE */

#ifdef MODULE
int init_module(void)
{
    if (debug)                  /* Emit version even if no cards detected. */
        printk(KERN_INFO "%s", version);
    return pci_drv_register(&via_rhine_drv_id, NULL);
}

void cleanup_module(void)
{
    struct net_device *next_dev;
//    int i;

    pci_drv_unregister(&via_rhine_drv_id);

    /* No need to check MOD_IN_USE, as sys_delete_module() checks. */
    while (root_net_dev) {
        netdev_private *np = (void *)(root_net_dev->priv);
        unregister_netdev(root_net_dev);
#ifdef USE_IO_OPS
        release_region(root_net_dev->base_addr, pci_tbl[np->chip_idx].io_size);
#else
        iounmap((char *)(root_net_dev->base_addr));
#endif
        next_dev = np->next_module;
#if (LINUX_VERSION_CODE >= 0x20341)
    if (np->ring) {
        if (np->revision >= 0x80)
            pci_free_consistent(np->pdev,(sizeof(rx_desc) * RX_RING_SIZE)+(sizeof(tx_desc) * TX_RING_SIZE * 8), np->ring, np->ring_dma);
        else
            pci_free_consistent(np->pdev,(sizeof(rx_desc) * RX_RING_SIZE)+(sizeof(tx_desc) * TX_RING_SIZE), np->ring, np->ring_dma);
    }
#else
        if (np->priv_addr) {
            kfree(np->priv_addr);
//            printk("chenyp:kfree:np->priv_addr=%x\n",np->priv_addr);
        }
        if (np->priv_rd) {
            kfree(np->priv_rd);
//            printk("chenyp:kfree:np->priv_rd=%x\n",np->priv_rd);
        }
        if (np->priv_td) {
            kfree(np->priv_td);
//            printk("chenyp:kfree:np->priv_rd=%x\n",np->priv_td);
        }
#endif
        kfree(root_net_dev);
//        printk("chenyp:kfree:root_net_dev=%x\n",root_net_dev);

        root_net_dev = next_dev;
    }
}

#endif  /* ifdef MODULE*/
