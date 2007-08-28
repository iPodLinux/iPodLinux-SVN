/*
	Copyright (c) 2002, Micrel Kendin Operations

	Written 2002 by LIQUN RUAN

	This software may be used and distributed according to the terms of 
	the GNU General Public License (GPL), incorporated herein by reference.
	Drivers based on or derived from this code fall under the GPL and must
	retain the authorship, copyright and license notice. This file is not
	a complete program and may only be used when the entire operating
	system is licensed under the GPL.

	The author may be reached as lruan@kendin.com
	Micrel Kendin Operations
	486 Mercury Dr.
	Sunnyvale, CA 94085

	This driver is for Kendin's KS8695 SOHO Router Chipset as ethernet driver.

	Support and updates available at
	www.kendin.com/ks8695/

*/
#include "ks8695_drv.h"
#include "ks8695_chipdef.h"
#include "ks8695_ioctrl.h"
#include "ks8695_cache.h"

static int macReset(PADAPTER_STRUCT Adapter);
static void macConfigure(PADAPTER_STRUCT Adapter);
static void macConfigureFlow(PADAPTER_STRUCT Adapter, uint8_t bFlowCtrl);
static void macConfigureInterrupt(PADAPTER_STRUCT Adapter);

static void swConfigure(PADAPTER_STRUCT Adapter);
static void swCreateLookUpTable(PADAPTER_STRUCT Adapter);

static void gpioConfigure(PADAPTER_STRUCT Adapter);

/*
 * ks8695_ChipInit
 *	This function is used to do chip initialization.
 *
 * Argument(s)
 *	Adapter		pointer to ADAPTER_STRUCT structure.
 *	bResetPhy	flag indicates whether to reset PHY as well
 *
 * Return(s)
 *	TRUE	if success
 *	FALSE	otherwise
 */
BOOLEAN ks8695_ChipInit(PADAPTER_STRUCT Adapter, BOOLEAN bResetPhy)
{
	BOOLEAN bStatus;
	struct net_device *netdev = Adapter->netdev;
	UINT i;

#ifdef	DEBUG_THIS
	DRV_INFO("%s", __FUNCTION__);
#endif

	if (bResetPhy) {
		/* phy related initialization */
		i = 0;
		swPhyReset(Adapter, i);
		if (DMA_LAN == DI.usDMAId) {
			for (i = 1; i < SW_MAX_LAN_PORTS; i++) {
				swPhyReset(Adapter, i);
				/* turn off GPIO, to make sure that if there is no cable connection, no light */
				gpioSet(Adapter, i, FALSE);
			}
		}
		swAutoNegoAdvertisement(Adapter, 0);
		if (DMA_LAN == DI.usDMAId) {
			for (i = 1; i < SW_MAX_LAN_PORTS; i++) {
				swAutoNegoAdvertisement(Adapter, i);
			}
		}
	}

	/* setup mac related stuff */
	bStatus = macReset(Adapter);
	if (bStatus) {
		DRV_ERR("%s: macReset failed", __FUNCTION__);
		return FALSE;
	}
	macConfigure(Adapter);
	macConfigureInterrupt(Adapter);

	/* turn off GPIO, to make sure that if there is no cable connection, no light */
	gpioSet(Adapter, 0, FALSE);
	/* configuration switch related to LAN only */
	if (DMA_LAN == DI.usDMAId) {
		swConfigure(Adapter);
		swCreateLookUpTable(Adapter);
		for (i = 1; i < SW_MAX_LAN_PORTS; i++) {
			/* turn off GPIO, to make sure that if there is no cable connection, no light */
			gpioSet(Adapter, i, FALSE);
		}
	}

	/* copy the MAC address out of Station registers */
	macSetStationAddress(Adapter, DI.stMacStation);
	if (netdev->addr_len < MAC_ADDRESS_LEN)
		netdev->addr_len = MAC_ADDRESS_LEN;
	memcpy(netdev->dev_addr, DI.stMacStation, netdev->addr_len);
	memcpy(DI.stMacCurrent, netdev->dev_addr, netdev->addr_len);

#ifdef	DEBUG_THIS
	DRV_INFO("MAC address %02X:%02X:%02X:%02X:%02X:%02X", 
		DI.stMacStation[0], 
		DI.stMacStation[1],
		DI.stMacStation[2],
		DI.stMacStation[3], 
		DI.stMacStation[4],
		DI.stMacStation[5]); 
#endif

	gpioConfigure(Adapter);

	/*RLQ, 11/20/2002, fix based on AN112. Changing transmitter gain in KS8695 to improve
	  the calbe length at which the Ethernet can operate */
	KS8695_WRITE_REG(REG_WAN_PHY_CONTROL, 0x0000b000);

	return bStatus ? FALSE : TRUE;
}

#define	IRQ_WAN_LEVEL	12
#define	IRQ_LAN_LEVEL	10
#define	IRQ_HPNA_LEVEL	8

/* move to source file later */
static INTCFG stDMAIntCfg[32] = {
	{ 0, -1 },		/* -1 don't touch, it's not ours */
	{ 0, -1 },
	{ 0, -1 },
	{ 0, -1 },
	{ 0, -1 },
	{ 0, -1 },
	{ 0, -1 },
	{ 0, -1 },
	{ 0, -1 },
	{ 0, -1 },
	{ 0, -1 },
	{ 0, -1 },		/* bit 11 */

	{ 0, 0x0b },	/* bit 12, LAN */
	{ 0, 0x0b },
	{ 0, 0x0a },
	{ 0, 0x0a },
	{ 0, 0x0f },
	{ 0, 0x0f },

	{ 0, IRQ_HPNA_LEVEL },	/* bit 18, HPNA */
	{ 0, IRQ_HPNA_LEVEL },
	{ 0, IRQ_HPNA_LEVEL },
	{ 0, IRQ_HPNA_LEVEL },
	{ 0, IRQ_HPNA_LEVEL },
	{ 0, IRQ_HPNA_LEVEL },

	{ 0, -1 },		/* bit 24 */

	{ 0, 0x0b },	/* bit 25, WAN */
	{ 0, 0x0b },
	{ 0, 0x0a },
	{ 0, 0x0a },
	{ 0, 0x0f },
	{ 0, 0x0f },
	{ 0, IRQ_WAN_LEVEL }	/* WAN link */
};

/*
 * macReset
 *	This function will execute a soft reset the chipset.
 *
 * Argument(s)
 *	Adapter		pointer to ADAPTER_STRUCT structure.
 *
 * Return(s)
 *	0		if success
 *	none zero otherwise
 */
int macReset(PADAPTER_STRUCT Adapter)
{
	int nTimeOut = 1000;
	UINT32 uReg;
	unsigned long flags;

#ifdef	DEBUG_THIS
	DRV_INFO("%s", __FUNCTION__);
#endif

	spin_lock_irqsave(&DI.lock, flags);
	/*spin_lock(&DI.lock);*/
	/* disable IER if any */
	uReg = KS8695_READ_REG(REG_INT_ENABLE);
	switch (DI.usDMAId) {
	case DMA_HPNA:
		uReg &= ~INT_HPNA_MASK;
		break;

	case DMA_LAN:
		uReg &= ~INT_LAN_MASK;
		break;

	default:
	case DMA_WAN:
		uReg &= ~INT_WAN_MASK;
		break;
	}
	KS8695_WRITE_REG(REG_INT_ENABLE, uReg);
	/*spin_unlock(&DI.lock);*/
	spin_unlock_irqrestore(&DI.lock, flags);

	/* reset corresponding register and wait for completion */
	KS8695_WRITE_REG(REG_TXCTRL + DI.nOffset, DMA_SOFTRESET);
	do
	{
		DelayInMilliseconds(1);
		uReg = KS8695_READ_REG(REG_TXCTRL + DI.nOffset);
		if (!(uReg & DMA_SOFTRESET))
			break;
	} while (--nTimeOut);

	if (nTimeOut < 1) {
		DRV_ERR("%s> timeout error", __FUNCTION__);
		return -ETIMEDOUT;
	}

	/* clear statistic counters */
	swResetSNMPInfo(Adapter);
#ifdef	DEBUG_THIS
	DRV_INFO("%s> succeeded (timeout count=%d)", __FUNCTION__, nTimeOut);
#endif

	return 0;
}

/*
 * macConfigure
 *	This function is used to set MAC control register based on configurable
 *  option settings.
 *
 * Argument(s)
 *	Adapter		pointer to ADAPTER_STRUCT structure.
 *
 * Return(s)
 *	NONE
 */
void macConfigure(PADAPTER_STRUCT Adapter)
{
	UINT uRxReg, uTxReg;

#ifdef	DEBUG_THIS
	DRV_INFO("%s", __FUNCTION__);
#endif

	/* read TX mode register */
	uTxReg = KS8695_READ_REG(REG_TXCTRL + DI.nOffset);
	/* clear PBL bits first */
	uTxReg &= ~DMA_PBLTMASK;				/* 29:24 */
	if (DI.byTxPBL)
		uTxReg |= ((uint32_t)DI.byTxPBL << DMA_PBLTSHIFT);

	if (DI.bTxChecksum) {
		uTxReg |= (DMA_IPCHECKSUM | DMA_TCPCHECKSUM | DMA_UDPCHECKSUM);
	}
	else {
		uTxReg &= ~(DMA_IPCHECKSUM | DMA_TCPCHECKSUM | DMA_UDPCHECKSUM);
	}

	if (DI.bTxFlowCtrl)
	{
		uTxReg |= DMA_FLOWCTRL;
	}
	else {
		uTxReg &= ~DMA_FLOWCTRL;
	}
	uTxReg |= (DMA_PADDING | DMA_CRC);
	/* write TX mode register */
	KS8695_WRITE_REG(REG_TXCTRL + DI.nOffset, uTxReg);

	/* read RX mode register */
	uRxReg = KS8695_READ_REG(REG_RXCTRL + DI.nOffset);
	/* clear PBL bits first */
	uRxReg &= ~DMA_PBLTMASK;				/* 29:24 */
	if (DI.byRxPBL)
		uRxReg |= ((uint32_t)DI.byRxPBL << DMA_PBLTSHIFT);
	/* checksum */
	if (DI.bRxChecksum) {
		uRxReg |= (DMA_IPCHECKSUM | DMA_TCPCHECKSUM | DMA_UDPCHECKSUM);
	}
	else {
		uRxReg &= ~(DMA_IPCHECKSUM | DMA_TCPCHECKSUM | DMA_UDPCHECKSUM);
	}
	/* flow control */
	if (DI.bRxFlowCtrl)
	{
		uRxReg |= DMA_FLOWCTRL;
	}
	else {
		uRxReg &= ~DMA_FLOWCTRL;
	}
	/* set unicast only, and let ks8695_set_multi function to set the rest */
	uRxReg |= DMA_UNICAST;

	/* write RX mode register */
	KS8695_WRITE_REG(REG_RXCTRL + DI.nOffset, uRxReg);
}

/*
 * macConfigureFlow
 *	This function is used to set mac flow control as a workaround for WAN port.
 *  option settings.
 *
 * Argument(s)
 *	Adapter		pointer to ADAPTER_STRUCT structure.
 *	bFlowCtrl	flow control to set
 *
 * Return(s)
 *	NONE
 */
void macConfigureFlow(PADAPTER_STRUCT Adapter, uint8_t bFlowCtrl)
{
	UINT uRxReg, uTxReg;
	BOOLEAN bTxStarted, bRxStarted;

#ifdef	DEBUG_THIS
	DRV_INFO("%s: flowctrl = %d", __FUNCTION__, bFlowCtrl);
#endif

	/* need to stop mac engines if started */
	bTxStarted = DI.bTxStarted;
	bRxStarted = DI.bRxStarted;
	if (bRxStarted)
		macStartRx(Adapter, FALSE);
	if (bTxStarted)
		macStartTx(Adapter, FALSE);

	/* read TX mode register */
	uTxReg = KS8695_READ_REG(REG_TXCTRL + DI.nOffset);
	if (bFlowCtrl)
	{
		uTxReg |= DMA_FLOWCTRL;
	}
	else {
		uTxReg &= ~DMA_FLOWCTRL;
	}
	/* write TX mode register */
	KS8695_WRITE_REG(REG_TXCTRL + DI.nOffset, uTxReg);

	/* read RX mode register */
	uRxReg = KS8695_READ_REG(REG_RXCTRL + DI.nOffset);
	if (bFlowCtrl)
	{
		uRxReg |= DMA_FLOWCTRL;
	}
	else {
		uRxReg &= ~DMA_FLOWCTRL;
	}
	/* write RX mode register */
	KS8695_WRITE_REG(REG_RXCTRL + DI.nOffset, uRxReg);

	if (bRxStarted)
		macStartRx(Adapter, TRUE);
	if (bTxStarted)
		macStartTx(Adapter, TRUE);
}

/*
 * macSetLoopback
 *	This function is used to set MAC lookback mode (for debugging purpose)
 *
 * Argument(s)
 *	Adapter		pointer to ADAPTER_STRUCT structure.
 *
 * Return(s)
 *	NONE
 */
void macSetLoopback(PADAPTER_STRUCT Adapter, BOOLEAN bLoopback)
{
	UINT uTxReg;
	BOOLEAN bTxStarted, bRxStarted;

	bTxStarted = DI.bTxStarted;
	bRxStarted = DI.bRxStarted;
	if (bRxStarted)
		macStartRx(Adapter, FALSE);
	if (bTxStarted)
		macStartTx(Adapter, FALSE);

	/* read TX mode register */
	uTxReg = KS8695_READ_REG(REG_TXCTRL + DI.nOffset);

	if (bLoopback)
	{
		uTxReg |= DMA_LOOPBACK;
	}
	else {
		uTxReg &= ~DMA_LOOPBACK;
	}

	/* write TX mode register */
	KS8695_WRITE_REG(REG_TXCTRL + DI.nOffset, uTxReg);

	if (bRxStarted)
		macStartRx(Adapter, TRUE);
	if (bTxStarted)
		macStartTx(Adapter, TRUE);
}

/*
 *  macStartRx
 *      This routine will start/stop RX machine.
 *
 *  Inputs:
 *      Adapter		pointer to ADAPTER_STRUCT data structure.
 *		bStart		TRUE if start Rx machine, FALSE if stop it
 *
 *  Returns:
 *      NONE
 */
void macStartRx(PADAPTER_STRUCT Adapter, BOOLEAN bStart)
{
	UINT32 uReg;

	uReg = KS8695_READ_REG(REG_RXCTRL + DI.nOffset);
	if (bStart) {
		/* start RX machine */
		uReg |= DMA_START;
		DI.bRxStarted = FALSE;
	} else {
		/* Stop RX machine */
		uReg &= ~DMA_START;
		DI.bRxStarted = FALSE;
	}
	KS8695_WRITE_REG(REG_RXCTRL + DI.nOffset, uReg);

	/* if there descriptors available for rx, tick off Rx engine */
	if (bStart) {
		if (atomic_read(&DI.RxDescEmpty) < DI.nRxDescTotal) {
			KS8695_WRITE_REG(REG_RXSTART + DI.nOffset, 1);
		}
	}
	else {
		/* clear corresponding ISR bits after stopped */
		KS8695_WRITE_REG(REG_INT_STATUS, DI.uIntMask);
	}
}

/*
 *  macStartTx
 *      This routine will start/stop TX machine.
 *
 *  Inputs:
 *      Adapter		pionter to ADAPTER_STRUCT data structure.
 *		bStart		TRUE if start Tx machine, FALSE if stop it
 *
 *  Returns:
 *      NONE
 */
void macStartTx(PADAPTER_STRUCT Adapter, BOOLEAN bStart)
{
	UINT32 uReg;

	uReg = KS8695_READ_REG(REG_TXCTRL + DI.nOffset);
	if (bStart) {
		/* start TX machine */
		uReg |= DMA_START;
		KS8695_WRITE_REG(REG_TXCTRL + DI.nOffset, uReg);
		DI.bTxStarted = FALSE;
		/* clear corresponding ISR bits after stopped */
		KS8695_WRITE_REG(REG_INT_STATUS, DI.uIntMask);
	} else {
		/* Stop TX machine */
		uReg &= ~DMA_START;
		KS8695_WRITE_REG(REG_TXCTRL + DI.nOffset, uReg);
		DelayInMilliseconds(2);
		DI.bTxStarted = FALSE;
	}
}

/*
 * macStopAll
 *	This function is use to stop both Tx/Rx.
 *
 * Argument(s)
 *      Adapter		pionter to ADAPTER_STRUCT data structure.
 *
 * Return(s)
 *	NONE
 */
void macStopAll(PADAPTER_STRUCT Adapter)
{
	/* stop Rx and Tx */
	macStartRx(Adapter, FALSE);
	macStartTx(Adapter, FALSE);
	
	/* disable interrupt!!! */
	macEnableInterrupt(Adapter, FALSE);
}

/*
 * macSetStationEx
 *	This function is use to set extra MAC station address
 *
 * Argument(s)
 *      Adapter		pionter to ADAPTER_STRUCT data structure.
 *		pMac		pointer to mac address buffer (should be 6)
 *		uIndex		index of extra mac address to set
 *
 * Return(s)
 *	0				if success
 *	negative value	if failed
 */
int macSetStationEx(PADAPTER_STRUCT Adapter, UCHAR *pMac, UINT uIndex)
{
#ifdef	DEBUG_THIS
	DRV_INFO("%s", __FUNCTION__);
#endif

	/* do we need to set multicast addr to extra station registers? */
	if (uIndex < MAC_MAX_EXTRA) {
		uint32_t uLowAddress, uHighAddress;

		uLowAddress = (*(pMac + 5)		|
					  (*(pMac + 4) << 8)	|
					  (*(pMac + 3) << 16)|
					  *(pMac + 2) << 24);

		uHighAddress = (*(pMac + 1)		|
					   *pMac << 8);

		/* make sure mac address is not all zero */
		if (uLowAddress | uHighAddress) {
			/* enable additional MAC */
			uHighAddress |= DMA_MACENABLE;
			KS8695_WRITE_REG(REG_MAC0_LOW + DI.nOffset + uIndex * 8, uLowAddress);
			KS8695_WRITE_REG(REG_MAC0_HIGH + DI.nOffset + uIndex * 8, uHighAddress);
			return 0;
		}
	}

	return ~EINVAL;
}

/*
 * macResetStationEx
 *	This function is use to clear extra MAC station address if set before
 *
 * Argument(s)
 *      Adapter		pionter to ADAPTER_STRUCT data structure.
 *		nIndex		index of extra mac address to set
 *		pMac		pointer to mac address buffer (should be 6)
 *
 * Return(s)
 *	0				if success
 *	negative value	if failed
 */
int macResetStationEx(PADAPTER_STRUCT Adapter, UCHAR *pMac)
{
	int	i, j;
	uint32_t uLowAddress, uHighAddress;

#ifdef	DEBUG_THIS
	DRV_INFO("%s", __FUNCTION__);
#endif

	uLowAddress = (*(pMac + 5)		|
				  (*(pMac + 4) << 8)	|
				  (*(pMac + 3) << 16)|
				  *(pMac + 2) << 24);

	uHighAddress = (*(pMac + 1)		|
				   *pMac << 8);

	/* set mac enable bit for comparison purpose only! */
	uHighAddress |= DMA_MACENABLE;

	/* if match is found, remove it (set to 0) */
	for (i = 0; i < MAC_MAX_EXTRA; i++) {
		j = i * 8;
		if (uLowAddress == KS8695_READ_REG(REG_MAC0_LOW + DI.nOffset + j) &&
			uHighAddress == KS8695_READ_REG(REG_MAC0_HIGH + DI.nOffset + j)) {
			KS8695_WRITE_REG(REG_MAC0_LOW + DI.nOffset + j, 0);
			KS8695_WRITE_REG(REG_MAC0_HIGH + DI.nOffset + j, 0);
		}
	}

    return 0;
}

/*
 * macGetIndexStationEx
 *	This function is use to get the index of empty station
 *
 * Argument(s)
 *      Adapter		pionter to ADAPTER_STRUCT data structure.
 *
 * Return(s)
 *	nIndex			index of empty to use
 *	negative value	if it is full
 */
int macGetIndexStationEx(PADAPTER_STRUCT Adapter)
{
	int	i;

#ifdef	DEBUG_THIS
	DRV_INFO("%s", __FUNCTION__);
#endif

	for (i = 0; i < MAC_MAX_EXTRA; i++) {
		if (!(KS8695_READ_REG(REG_MAC0_HIGH + DI.nOffset + i * 8) & DMA_MACENABLE)) {
			return i;
		}
	}

	DRV_WARN("%s: no empty slot for Additional Station Address", __FUNCTION__);
    return -1;
}

/* Interrupt Bit definitions */
#define	IB_WAN_LINK			31
#define	IB_WAN_RX_STOPPED	25
#define	IB_HPNA_TX			23
#define	IB_HPNA_RX_STOPPED	18
#define	IB_LAN_TX			17
#define	IB_LAN_RX_STOPPED	12

/*
 *  macConfigureInterrupt
 *      This routine is used to configure interrupt priority
 *
 *  Inputs:
 *      Adapter		pionter to ADAPTER_STRUCT data structure.
 *
 *  Returns:
 *      NONE
 */
void macConfigureInterrupt(PADAPTER_STRUCT Adapter)
{
	int	i;
	UINT	uIMR, uIPR = 0;
	unsigned long flags;

#ifdef	DEBUG_THIS
	DRV_INFO("%s", __FUNCTION__);
#endif

	spin_lock_irqsave(&DI.lock, flags);
	/*spin_lock(&DI.lock);*/
	uIMR = KS8695_READ_REG(REG_INT_CONTL);
	switch (DI.usDMAId) {
		case DMA_LAN:
			for (i = IB_LAN_RX_STOPPED; i <= IB_LAN_TX; i++) {
				if (stDMAIntCfg[i].bFIQ) {
					uIMR |= (1L << i);
					DI.bUseFIQ = TRUE;
				}
				else {
					uIMR &= ~(1L << i);
					uIPR |= ((UINT)(stDMAIntCfg[i].byPriority) & 0xf) << (i - IB_LAN_RX_STOPPED + 1) * 4;
				}
			}
			KS8695_WRITE_REG(REG_INT_LAN_PRIORITY, uIPR);
			break;

		case DMA_HPNA:
			for (i = IB_HPNA_RX_STOPPED; i <= IB_HPNA_TX; i++) {
				if (stDMAIntCfg[i].bFIQ) {
					uIMR |= (1L << i);
					DI.bUseFIQ = TRUE;
				}
				else {
					uIMR &= ~(1L << i);
					uIPR |= ((UINT)(stDMAIntCfg[i].byPriority) & 0xf) << (i - IB_HPNA_RX_STOPPED + 1) * 4;
				}
			}
			KS8695_WRITE_REG(REG_INT_HPNA_PRIORITY, uIPR);
			break;

		case DMA_WAN:
		default:
			for (i = IB_WAN_RX_STOPPED; i <= IB_WAN_LINK; i++) {
				if (stDMAIntCfg[i].bFIQ) {
					uIMR |= (1L << i);
					DI.bUseFIQ = TRUE;
				}
				else {
					uIMR &= ~(1L << i);
					uIPR |= ((UINT)(stDMAIntCfg[i].byPriority) & 0xf) << (i - IB_WAN_RX_STOPPED + 1) * 4;
				}
			}
			KS8695_WRITE_REG(REG_INT_WAN_PRIORITY, uIPR);
			break;
	}
	KS8695_WRITE_REG(REG_INT_CONTL, uIMR);
	/*spin_unlock(&DI.lock);*/
	spin_unlock_irqrestore(&DI.lock, flags);
}

/*
 *  macEnableInterrupt
 *      This routine is used to enable/disable interrupt related to MAC only
 *
 *  Inputs:
 *      Adapter		pionter to ADAPTER_STRUCT data structure.
 *		bEnable		enable/disable interrupt
 *
 *  Returns:
 *      NONE
 */
void macEnableInterrupt(PADAPTER_STRUCT Adapter, BOOLEAN bEnable)
{
	UINT	uIER;

	spin_lock(&DI.lock);
	uIER = KS8695_READ_REG(REG_INT_ENABLE);
	switch (DI.usDMAId) {
		case DMA_LAN:
		case DMA_HPNA:
			if (bEnable)
				uIER |= DI.uIntMask;
			else
				uIER &= ~DI.uIntMask;
			break;

		case DMA_WAN:
			if (bEnable)
				uIER |= (DI.uIntMask | DI.uLinkIntMask);
			else
				uIER &= ~(DI.uIntMask | DI.uLinkIntMask);
			break;

		default:
			DRV_INFO("unsupported option");
			break;
	}
	KS8695_WRITE_REG(REG_INT_ENABLE, uIER);
	spin_unlock(&DI.lock);
	ks8695_power_saving(bEnable);
}

/*
 * macGetStationAddress
 *	This function reads MAC address from station address registers.
 *
 * Argument(s)
 *  Adapter		pointer to ADAPTER_STRUCT struct
 *  pMacAddress	pointer to a byte array to hold MAC address (at least 6 bytes long)
 *
 * Return(s)
 *	NONE.
 */
void macGetStationAddress(PADAPTER_STRUCT Adapter, uint8_t *pMacAddress)
{
	UINT32 uTmp;
	int i;

#ifdef	DEBUG_THIS
	DRV_INFO("%s", __FUNCTION__);
#endif

	/* read low 4 buytes, byte order (e.g. in our NIC card, 00101a00000d) */
	uTmp = KS8695_READ_REG(REG_STATION_LOW + DI.nOffset);
	for (i = (MAC_ADDRESS_LEN - 1); i > 1; i--)
	{
		pMacAddress[i] = (UCHAR)(uTmp & 0x0ff);
		uTmp >>= 8;
	}
	/* read high 2 bytes */
	uTmp = KS8695_READ_REG(REG_STATION_HIGH + DI.nOffset);
	pMacAddress[1] = (UCHAR)(uTmp & 0x0ff);
	uTmp >>= 8;
	pMacAddress[0] = (UCHAR)(uTmp & 0x0ff);
}

/*
 * macSetStationAddress
 *	This function sets MAC address to given type (WAN, LAN or HPHA)
 *
 * Argument(s)
 *  Adapter		pointer to ADAPTER_STRUCT struct
 *  pMacAddress	pointer to a byte array to hold MAC address (at least 6 bytes long)
 *
 * Return(s)
 *	NONE.
 */
void macSetStationAddress(PADAPTER_STRUCT Adapter, uint8_t *pMacAddress)
{
	UINT32 uLow, uHigh;

#ifdef	DEBUG_THIS
	DRV_INFO("%s", __FUNCTION__);
#endif

	uLow = ((UINT32)pMacAddress[2] << 24);
	uLow += ((UINT32)pMacAddress[3] << 16);
	uLow += ((UINT32)pMacAddress[4] << 8);
	uLow += pMacAddress[5];
	uHigh = ((UINT32)pMacAddress[0] << 8) + pMacAddress[1];
	KS8695_WRITE_REG(REG_STATION_LOW + DI.nOffset, uLow);
	KS8695_WRITE_REG(REG_STATION_HIGH + DI.nOffset, uHigh);
}

/*
 * swConfigurePort
 *	This function is used to configure a give port for LAN.
 *
 * Argument(s)
 *  Adapter		pointer to ADAPTER_STRUCT struct
 *  uPort		port to start
 *
 * Return(s)
 *	NONE.
 */
void swConfigurePort(PADAPTER_STRUCT Adapter, UINT uPort)
{
	UINT	uReg, uOff;
	BOOLEAN	bPort5 = FALSE;

	if (uPort >= SW_MAX_LAN_PORTS) {
		if (SW_MAX_LAN_PORTS == uPort) {
			/* port 5 */
			bPort5 = TRUE;
		}
		else {
			/* out of range */
			DRV_INFO("%s: port %d to configure out of range", __FUNCTION__, uPort);
			return;
		}
	}
	uOff = REG_SWITCH_PORT1 + uPort * 4;

	uReg = 0;
	uReg |= (UINT)DPI[uPort].usTag << 16;

	if (!bPort5) {
		/* connection media type */
		/*uReg &= ~(SW_PORT_DISABLE_AUTONEG | SW_PORT_100BASE | SW_PORT_FULLDUPLEX);*/
		if (SW_PHY_AUTO != DI.usCType[uPort]) {
			uReg |= SW_PORT_DISABLE_AUTONEG;
			if (SW_PHY_100BASE_TX == DI.usCType[uPort] || 
				SW_PHY_100BASE_TX_FD == DI.usCType[uPort]) {
				uReg |= SW_PORT_100BASE;
			}
			if (SW_PHY_10BASE_T_FD == DI.usCType[uPort] || 
				SW_PHY_100BASE_TX_FD == DI.usCType[uPort]) {
				uReg |= SW_PORT_FULLDUPLEX;
			}
		}
	}
	else {
		/* Rx direct mode */
		if (DI.bRxDirectMode) {
			uReg |= SW_PORT_RX_DIRECT_MODE;
		}
		/* Tx Pre-tag mode */
		if (DI.bTxRreTagMode) {
			uReg |= SW_PORT_TX_PRETAG_MODE;
		}
	}

	/* cross talk bit mask */
	uReg |= ((UINT)(DPI[uPort].byCrossTalkMask & 0x1f)) << 8;

	/* spanning tree */
	if (SW_SPANNINGTREE_ALL == DPI[uPort].bySpanningTree) {
		uReg |= SW_PORT_TX_SPANNINGTREE | SW_PORT_RX_SPANNINGTREE;
	}
	else {
		if (SW_SPANNINGTREE_TX == DPI[uPort].bySpanningTree) {
			uReg |= SW_PORT_TX_SPANNINGTREE;
		}
		if (SW_SPANNINGTREE_RX == DPI[uPort].bySpanningTree) {
			uReg |= SW_PORT_RX_SPANNINGTREE;
		}
	}
	if (DPI[uPort].byDisableSpanningTreeLearn) {
		uReg |= SW_PORT_NO_SPANNINGTREE;
	}
	/* ingress broadcast storm protection */
	if (DPI[uPort].byStormProtection) {
		uReg |= SW_PORT_STORM_PROCTION;
	}
	/* ingress priority */
	if (DPI[uPort].byIngressPriority) {
		uReg |= SW_PORT_HI_PRIORITY;
	}
	if (DPI[uPort].byIngressPriorityTOS) {
		uReg |= SW_PORT_TOS_ENABLE;
	}
	if (DPI[uPort].byIngressPriority802_1P) {
		uReg |= SW_PORT_8021Q_ENABLE;
	}
	/* egress priority */
	if (DPI[uPort].byEgressPriority) {
		uReg |= SW_PORT_PRIOTIRY_ENABLE;
	}
	KS8695_WRITE_REG(uOff, uReg);
	/* need 20 cpu clock delay for switch related registers */
	DelayInMicroseconds(10);

#ifdef	DEBUG_THIS
	DRV_INFO("%s: uOff=0x%08x, reg=0x%08x", __FUNCTION__, uOff, uReg);
#endif
}

/*
 * swEnableSwitch
 *	This function is used to enable/disable switch
 *
 * Argument(s)
 *  Adapter		pointer to ADAPTER_STRUCT struct
 *  enable		enable/disable switch
 *
 * Return(s)
 *	NONE.
 */
void swEnableSwitch(PADAPTER_STRUCT Adapter, UINT enable)
{
	UINT	uReg;

	uReg = KS8695_READ_REG(REG_SWITCH_CTRL0);
	if (enable) {
		uReg |= SW_CTRL0_SWITCH_ENABLE;
		/* for debug purpose */
		/*uReg |= 0x00080000;*/
	} else {
		uReg &= ~SW_CTRL0_SWITCH_ENABLE;
		/* for debug purpose */
		/*uReg &= ~0x00080000;*/
	}

	KS8695_WRITE_REG(REG_SWITCH_CTRL0, uReg);
	/* need 20 cpu clock delay for switch related registers */
	DelayInMicroseconds(10);
}

/*
 * swReadSNMPReg
 *	This function is used to read SNMP registers
 *
 * Argument(s)
 *	Adapter		pointer to ADAPTER_STRUCT structure.
 *	uIndex		index of SNMP register to read
 *
 * Return(s)
 *	value read
 */
UINT swReadSNMPReg(PADAPTER_STRUCT Adapter, UINT uIndex)
{
	UINT	uValue, uTimeout = 0;

	if (uIndex >= 512)
		uIndex = 511;

    KS8695_WRITE_REG(REG_MANAGE_COUNTER, uIndex);
	DelayInMicroseconds(10);
	do {
		uValue = KS8695_READ_REG(REG_MANAGE_DATA);
		if (uValue & SW_SNMP_DATA_VALID) {
			if (uValue & SW_SNMP_DATA_OVERFLOW) {
				KS8695_WRITE_REG(REG_MANAGE_DATA, SW_SNMP_DATA_OVERFLOW);
			}
			/* clear status bits */
			uValue &= 0x3fffffff;
			return uValue;
		}
		DelayInMilliseconds(1);
	} 
	while (uTimeout++ < 2000);

	if (uValue & SW_SNMP_DATA_OVERFLOW) {
		KS8695_WRITE_REG(REG_MANAGE_DATA, SW_SNMP_DATA_OVERFLOW);
	}
    return 0;
}

/*
 * swConfigure
 *	This function is used to config switch engine. It is assume that
 *	the BIST is performed already (by boot loader).
 *
 * Argument(s)
 *	Adapter		pointer to ADAPTER_STRUCT structure.
 *
 * Return(s)
 *	NONE
 */
void swConfigure(PADAPTER_STRUCT Adapter)
{
	UINT uReg, i;

#ifdef	DEBUG_THIS
	DRV_INFO("%s", __FUNCTION__);
#endif

	if (DMA_LAN == DI.usDMAId) {
		/* read switch control 0 register */
		uReg = KS8695_READ_REG(REG_SWITCH_CTRL0);
		/* flow control for LAN ports */
		if (DI.bPort5FlowCtrl)
		{
			/* flow control for port 5 */
			uReg |= SW_CTRL0_ENABLE_PORT5;
		}
		else {
			uReg &= ~SW_CTRL0_ENABLE_PORT5;
		}
		if (DI.bPortsFlowCtrl)
		{
			/* four flow control for each LAN port */
			uReg |= SW_CTRL0_ENABLE_PORTS;
		}
		else {
			uReg &= ~SW_CTRL0_ENABLE_PORTS;
		}

		/*RLQ, 11/20/2002, requested by Hock, backpressure will fix packet drop problem
		  in half duplex mode */
		uReg |= 0x00000020;		/* bit 5 */

		/* set flow control fairness mode based on LAN flow control settings, should use */

		/* read switch control 0 register */
		KS8695_WRITE_REG(REG_SWITCH_CTRL0, uReg);
		/* need 20 cpu clock delay for switch related registers */
		DelayInMicroseconds(10);
		
		/* configure LAN port 1-4 and Port 5 */
		for (i = 0; i <= SW_MAX_LAN_PORTS; i++) 
			swConfigurePort(Adapter, i);
	}
	else {
		DRV_INFO("%s: type (%x) not supported", __FUNCTION__, DI.usDMAId);
	}
}

/*
 * swSetLED
 *	This function is used to set given LED
 *
 * Argument(s)
 *  Adapter		pointer to ADAPTER_STRUCT struct
 *  bLED1		TRUE for LED1 and FALSE for LED0
 *	nSel		emum type LED_SELECTOR
 *
 * Return(s)
 *	NONE.
 */
void swSetLED(PADAPTER_STRUCT Adapter, BOOLEAN bLED1, LED_SELECTOR nSel)
{
	UINT32 uReg;

	switch (DI.usDMAId) {
	case DMA_HPNA:
		/* there is no LED for HPNA */
		break;

	case DMA_WAN:
		uReg = KS8695_READ_REG(REG_WAN_CONTROL);
		if (bLED1) {
			uReg &= 0xffffff8f;		/* 6:4 */
			uReg |= (UINT)(nSel & 0x07) << 4;
		}
		else {
			uReg &= 0xfffffff8;		/* 2:0 */
			uReg |= (UINT)(nSel & 0x07);
		}
#if 0
		KS8695_WRITE_REG(REG_WAN_CONTROL, uReg);
#endif
		/* need 20 cpu clock delay for switch related registers */
		DelayInMicroseconds(10);
		return;

	default:
	case DMA_LAN:
		uReg = KS8695_READ_REG(REG_SWITCH_CTRL0);
		if (bLED1) {
			uReg &= 0xf1ffffff;		/* 27:25 */
			uReg |= (UINT)(nSel & 0x07) << 25;
		}
		else {
			uReg &= 0xfe3fffff;		/* 24:22 */
			uReg |= (UINT)(nSel & 0x07) << 22;
		}
#if 0
		KS8695_WRITE_REG(REG_SWITCH_CTRL0, uReg);
#endif
		/* need 20 cpu clock delay for switch related registers */
		DelayInMicroseconds(10);
		break;
	}
}

/*
 * swAutoNegoStart
 *	This function is used to start auto negotiation process
 *
 * Argument(s)
 *  Adapter		pointer to ADAPTER_STRUCT struct
 *  uPort		port to start
 *
 * Return(s)
 *	NONE.
 */
void swAutoNegoStart(PADAPTER_STRUCT Adapter, UINT uPort)
{
	UINT	uReg, uOff, uShift = 0;

#ifdef	DEBUG_THIS
	DRV_INFO("%s: port=%d, bAutoNegoInProgress=%d", __FUNCTION__, uPort, DI.bAutoNegoInProgress[uPort]);
#endif

	switch (DI.usDMAId) {
	case DMA_HPNA:
		/* there is no auto nego for HPNA */
		return;

	case DMA_WAN:
		uOff = REG_WAN_CONTROL;
		uShift = 16;
		break;

	default:
	case DMA_LAN:
		switch (uPort) {
		case SW_PORT_4:
			uOff = REG_SWITCH_AUTO1;
			uShift = 16;
			break;

		case SW_PORT_3:
			uOff = REG_SWITCH_AUTO1;
			break;

		case SW_PORT_2:
			uOff = REG_SWITCH_AUTO0;
			uShift = 16;
			break;

		case SW_PORT_1:
		default:
			uOff = REG_SWITCH_AUTO0;
			break;
		}
		break;
	}

	/* if not auto nego */
	if (SW_PHY_AUTO != DI.usCType[uPort]) {
		return;
	}
	DI.bAutoNegoInProgress[uPort] = TRUE;
	uReg = KS8695_READ_REG(uOff);
	uReg |= ((UINT)SW_AUTONEGO_RESTART << uShift);
	KS8695_WRITE_REG(uOff, uReg);
	/* need 20 cpu clock delay for switch related registers */
	DelayInMicroseconds(10);
}

/*
 * swAutoNegoAdvertisement
 *	This function is used to set PHY Advertisement.
 *
 * Argument(s)
 *	Adapter		pointer to ADAPTER_STRUCT structure.
 *  uPort		port to advertise
 *
 * Return(s)
 *	NONE
 */
void swAutoNegoAdvertisement(PADAPTER_STRUCT Adapter, UINT uPort)
{
	UINT	uReg, uOff, uShift = 0;

#ifdef	DEBUG_THIS
    DRV_INFO("%s", __FUNCTION__);
#endif

	switch (DI.usDMAId) {
	case DMA_HPNA:
		/* there is no auto nego feature for HPNA DMA, but I'll assume that
		   if SW_PHY_AUTO is set, use 100/FD as default
		 */ 
		uReg = KS8695_READ_REG(REG_HPNA_CONTROL) & 0xfffffffc;
		if (SW_PHY_AUTO == DI.usCType[uPort]) {
			uReg |= 0x00000003;
		}
		else {
			if (SW_PHY_100BASE_TX == DI.usCType[uPort] || 
				SW_PHY_100BASE_TX_FD == DI.usCType[uPort]) {
				uReg |= 0x00000002;
			}
			if (SW_PHY_10BASE_T_FD == DI.usCType[uPort] || 
				SW_PHY_100BASE_TX_FD == DI.usCType[uPort]) {
				uReg |= 0x00000001;
			}
		}
		KS8695_WRITE_REG(REG_MISC_CONTROL, uReg);
		/* need 20 cpu clock delay for switch related registers */
		DelayInMicroseconds(10);
		return;

	case DMA_WAN:
		uOff = REG_WAN_CONTROL;
		uShift = 16;
		break;

	default:
	case DMA_LAN:
		switch (uPort) {
		case SW_PORT_4:
			uOff = REG_SWITCH_AUTO1;
			break;

		case SW_PORT_3:
			uOff = REG_SWITCH_AUTO1;
			uShift = 16;
			break;

		case SW_PORT_2:
			uOff = REG_SWITCH_AUTO0;
			break;

		case SW_PORT_1:
		default:
			uOff = REG_SWITCH_AUTO0;
			uShift = 16;
			break;
		}
		break;
	}
	uReg = KS8695_READ_REG(uOff);
	/* clear corresponding bits first */
	uReg &= ~(SW_AUTONEGO_ADV_MASK << uShift);
	if (SW_PHY_AUTO == DI.usCType[uPort])
	{
		uReg |= (SW_AUTONEGO_ADV_100FD | SW_AUTONEGO_ADV_100HD |
			SW_AUTONEGO_ADV_10FD | SW_AUTONEGO_ADV_10HD) << uShift;
	}
	else
	{
		/* Manually set */
		switch (DI.usCType[uPort])
		{
		case SW_PHY_100BASE_TX_FD:
			uReg |= SW_AUTONEGO_ADV_100FD << uShift;
			break;

		case SW_PHY_100BASE_TX:
			uReg |= SW_AUTONEGO_ADV_100HD << uShift;
			break;

		case SW_PHY_10BASE_T_FD:
			uReg |= SW_AUTONEGO_ADV_10FD << uShift;
			break;

		case SW_PHY_10BASE_T:
			uReg |= SW_AUTONEGO_ADV_10HD << uShift;
			break;

		default:
			/* Unsupported media type found! */
			DRV_WARN("%s> Unsupported media type found!", __FUNCTION__);
			return;
		}
	}

	if (DI.bRxFlowCtrl)
		uReg |= SW_AUTONEGO_ADV_PUASE << uShift;

	uReg &= ~((UINT)SW_AUTONEGO_RESTART << uShift);
	KS8695_WRITE_REG(uOff, uReg);
	/* need 20 cpu clock delay for switch related registers */
	DelayInMicroseconds(10);
}

/*
 * swGetWANLinkStatus
 *	This function is used to get WAN link status.
 *
 * Argument(s)
 *	Adapter		pointer to ADAPTER_STRUCT structure.
 *  uPort		port to query
 *
 * Return(s)
 *	TRUE	if link is up
 *	FALSE	if link is down
 */
BOOLEAN swGetWANLinkStatus(PADAPTER_STRUCT Adapter)
{
	UINT	uReg;

	uReg = KS8695_READ_REG(REG_WAN_CONTROL);
	/* if not linked yet */
	if (!(uReg & ((UINT)SW_AUTONEGO_STAT_LINK << 16))) {
#ifdef	DEBUG_THIS
		DRV_INFO("WAN link is down");
#endif
		return FALSE;
	}
#ifdef	DEBUG_THIS
	DRV_INFO("WAN link is up");
#endif

	return TRUE;
}

/*
 * swGetPhyStatus
 *	This function is used to get the status of auto negotiation.
 *
 * Argument(s)
 *	Adapter		pointer to ADAPTER_STRUCT structure.
 *  uPort		port to query
 *
 * Return(s)
 *	TRUE	if connected
 *	FALSE	otherwise
 */
int swGetPhyStatus(PADAPTER_STRUCT Adapter, UINT uPort)
{
	UINT	uReg, uOff, uShift = 0;

#ifdef	DEBUG_THIS
	DRV_INFO("%s", __FUNCTION__);
#endif

	switch (DI.usDMAId) {
	case DMA_HPNA:
		/* temp */
		uReg = KS8695_READ_REG(REG_MISC_CONTROL);
		DI.usLinkSpeed[uPort] = (uReg & 0x00000002) ? SPEED_100 : SPEED_10;
		DI.bHalfDuplex[uPort] = (uReg & 0x00000001) ? FULL_DUPLEX : HALF_DUPLEX;
		/* note that there is no register bit corresponding to HPNA's link status
		   therefore don't report it */
		DI.bLinkActive[uPort] = TRUE;
		return TRUE;

	case DMA_WAN:
		uOff = REG_WAN_CONTROL;
		uShift = 16;
		break;

	default:
	case DMA_LAN:
		switch (uPort) {
		case SW_PORT_4:
			uOff = REG_SWITCH_AUTO1;
			break;

		case SW_PORT_3:
			uOff = REG_SWITCH_AUTO1;
			uShift = 16;
			break;

		case SW_PORT_2:
			uOff = REG_SWITCH_AUTO0;
			break;

		case SW_PORT_1:
		default:
			uOff = REG_SWITCH_AUTO0;
			uShift = 16;
			break;
		}
	}

	uReg = KS8695_READ_REG(uOff);
	/* if not linked yet */
	if (!(uReg & ((UINT)SW_AUTONEGO_STAT_LINK << uShift))) {
		DI.bLinkActive[uPort] = FALSE;
		DI.usLinkSpeed[uPort] = SPEED_UNKNOWN;
		DI.bHalfDuplex[uPort] = 0;
		gpioSet(Adapter, uPort, FALSE);
		return FALSE;
	}
	DI.bLinkActive[uPort] = TRUE;
	if (SW_PHY_AUTO == DI.usCType[uPort]) {
		/* if auto nego complete */
		if ((UINT)SW_AUTONEGO_COMPLETE << uShift) {
			/* clear auto nego restart bit */
			uReg &= ~((UINT)SW_AUTONEGO_RESTART << uShift);
			KS8695_WRITE_REG(uOff, uReg);
			DelayInMicroseconds(10);

			DI.usLinkSpeed[uPort] = (uReg & ((UINT)SW_AUTONEGO_STAT_SPEED << uShift)) ? SPEED_100 : SPEED_10;
			DI.bHalfDuplex[uPort] = (uReg & ((UINT)SW_AUTONEGO_STAT_DUPLEX << uShift)) ? FULL_DUPLEX : HALF_DUPLEX;
			DI.bAutoNegoInProgress[uPort] = FALSE;
			
			gpioSet(Adapter, uPort, SPEED_100 == DI.usLinkSpeed[uPort]);
			/*RLQ, need to verify real duplex mode instead report it correct here */
			/* duplex bit may not right if partner doesn't support all mode, do further detection */
			if ((uReg & (SW_AUTONEGO_PART_100FD | SW_AUTONEGO_PART_100HD | SW_AUTONEGO_PART_10FD | SW_AUTONEGO_PART_10HD) << uShift)
				!= (SW_AUTONEGO_PART_100FD | SW_AUTONEGO_PART_100HD | SW_AUTONEGO_PART_10FD | SW_AUTONEGO_PART_10HD)) {
				if (SPEED_100 == DI.usLinkSpeed[uPort]) {
					if ((uReg & (SW_AUTONEGO_PART_100FD << uShift))) 
						DI.bHalfDuplexDetected[uPort] = FULL_DUPLEX;
					else
						DI.bHalfDuplexDetected[uPort] = HALF_DUPLEX;
				}
				else {
					if ((uReg & (SW_AUTONEGO_PART_10FD << uShift)))
						DI.bHalfDuplexDetected[uPort] = FULL_DUPLEX;
					else
						DI.bHalfDuplexDetected[uPort] = HALF_DUPLEX;
				}
			}

			/* software workaround for flow control, need to know partner's flow control */
			if (DMA_WAN == DI.usDMAId) {	/* currently do it to WAN only, there is no problem to LAN, will do HPNA later */
				uint8_t	bFlowCtrl;

				/* we need to check partner's control flow setting for the matching, if not, changes ours */
				bFlowCtrl = ((SW_AUTONEGO_PART_PAUSE << uShift) & uReg) ? TRUE : FALSE;
				if (bFlowCtrl != DI.bRxFlowCtrl) {	/* Tx same as Rx, so test Rx should be enough */
					/* need to change ours accordingly, which will overwrite current one */
					macConfigureFlow(Adapter, bFlowCtrl);
				}
			}
#ifdef	DEBUG_THIS
			DRV_INFO("%s> Auto Nego completed", __FUNCTION__);
#endif
		}
		else {
#ifdef	DEBUG_THIS
			/* auto nego in progress */
			DRV_INFO("%s> Auto Nego in progress...", __FUNCTION__);
#endif
			/* wait for next timer */
			DI.bLinkActive[uPort] = FALSE;
			DI.usLinkSpeed[uPort] = SPEED_UNKNOWN;
			DI.bHalfDuplex[uPort] = 0;
		}
	}
	else {
#ifdef	DEBUG_THIS
		DRV_INFO("%s: media type=%d, port=%d", __FUNCTION__, DI.usCType[uPort], uPort);
#endif

		/* manually connection */
		if (SW_PHY_10BASE_T_FD == DI.usCType[uPort] || SW_PHY_100BASE_TX_FD == DI.usCType[uPort]) {
			DI.bHalfDuplex[uPort] = FULL_DUPLEX;
		}
		else {
			DI.bHalfDuplex[uPort] = HALF_DUPLEX;
		}
		if (SW_PHY_100BASE_TX_FD == DI.usCType[uPort] || SW_PHY_100BASE_TX == DI.usCType[uPort]) {
			DI.usLinkSpeed[uPort] = SPEED_100;
			gpioSet(Adapter, uPort, TRUE);
		}
		else {
			DI.usLinkSpeed[uPort] = SPEED_10;
			gpioSet(Adapter, uPort, FALSE);
		}

		/* software workaround for flow control, need to know partner's flow control */
		if (DMA_WAN == DI.usDMAId) {	/* currently do it to WAN only, there is no problem to LAN, will do HPNA later */
			macConfigureFlow(Adapter, FULL_DUPLEX == DI.bHalfDuplex[uPort] ? TRUE : FALSE);
		}
	}
	return TRUE;
}

/*
 * swDetectPhyConnection
 *	This function is used to start auto negotiation
 *
 * Argument(s)
 *  Adapter		pointer to ADAPTER_STRUCT struct
 *  uPort		port to start
 *
 * Return(s)
 *	NONE.
 */
void swDetectPhyConnection(PADAPTER_STRUCT Adapter, UINT uPort)
{
	/*if (SW_PHY_AUTO == DI.usCType[uPort] && !DI.bAutoNegoInProgress[uPort] && DI.bLinkChanged[uPort]) {*/
	if (LINK_SELECTION_FORCED != DI.byDisableAutoNego[uPort] && !DI.bAutoNegoInProgress[uPort] && DI.bLinkChanged[uPort]) {
		swAutoNegoStart(Adapter, uPort);
		DI.bLinkChanged[uPort] = FALSE;
		DI.bLinkActive[uPort] = FALSE;
	}
	swGetPhyStatus(Adapter, uPort);
}

/*
 * swPhyReset
 *	This function is used to reset phy chipset (powerdown or soft reset).
 *
 * Argument(s)
 *	Adapter		pointer to ADAPTER_STRUCT structure.
 *  uPort		port to start
 *
 * Return(s)
 *	NONE
 */
void swPhyReset(PADAPTER_STRUCT Adapter, UINT uPort)
{
	UINT	uReg, uShift = 0;
	UINT	uPowerReg;
	/*RLQ, workaround */
	UINT	uReg1;

#ifdef	DEBUG_THIS
    DRV_INFO("%s", __FUNCTION__);
#endif

	/* IEEE spec. of auto nego bit */
	uReg1 = BIT(7);
	switch (DI.usDMAId) {
	case DMA_HPNA:
		return;

	case DMA_WAN:
		uPowerReg = REG_WAN_POWERMAGR;
		break;

	default:
	case DMA_LAN:
		switch (uPort) {
		case SW_PORT_4:
			uPowerReg = REG_LAN34_POWERMAGR;
			break;

		case SW_PORT_3:
			uPowerReg = REG_LAN34_POWERMAGR;
			uShift = 16;
			break;

		case SW_PORT_2:
			uPowerReg = REG_LAN12_POWERMAGR;
			break;

		case SW_PORT_1:
		default:
			uPowerReg = REG_LAN12_POWERMAGR;
			uShift = 16;
			break;
		}
	}

	if (DI.bPowerDownReset) {
		int	nCount = 0;
		
		uReg = KS8695_READ_REG(uPowerReg);
		KS8695_WRITE_REG(uPowerReg, uReg | ((UINT)POWER_POWERDOWN << uShift));
		/* need 20 cpu clock delay for switch related registers */
		/*DelayInMicroseconds(10);*/
		do {
			DelayInMilliseconds(50);
		} while (nCount++ < 4);
		uReg &= ~((UINT)POWER_POWERDOWN << uShift);
		/* turn off IEEE auto nego */
		uReg &= ~(uReg1 << uShift);
		KS8695_WRITE_REG(uPowerReg, uReg);
		/* need 20 cpu clock delay for switch related registers */
		DelayInMicroseconds(10);
	}
	else {
		uReg = KS8695_READ_REG(uPowerReg);
		/* turn off IEEE auto nego */
		uReg &= ~(uReg1 << uShift);
		KS8695_WRITE_REG(uPowerReg, uReg);
		/* need 20 cpu clock delay for switch related registers */
		DelayInMicroseconds(10);
	}
}

/*
 * swConfigureMediaType
 *	This function is used to set linke media type (forced)
 *
 * Argument(s)
 *	Adapter		pointer to ADAPTER_STRUCT structure.
 *  uPort		port to start
 *	uSpeed		media speed to set
 *	uDuplex		media duplex to set
 *
 * Return(s)
 *	TRUE	if succeeded
 *	FALSE	otherwise
 */
void swConfigureMediaType(PADAPTER_STRUCT Adapter, UINT uPort, UINT uSpeed, UINT uDuplex)
{
	UINT32 uReg, uOffset;

#ifdef	DEBUG_THIS
	DRV_INFO("%s", __FUNCTION__);
#endif

	switch (DI.usDMAId) {
	case DMA_HPNA:
		/* there is no way to force HPNA */
		return;

	case DMA_WAN:
		uOffset = REG_WAN_CONTROL;
		uPort = 0;
		break;

	default:
	case DMA_LAN:
		if (uPort >= SW_MAX_LAN_PORTS) {
			DRV_WARN("%s: port (%d) gave is out of range", __FUNCTION__, uPort);
			return;
		}
		uOffset = REG_SWITCH_PORT1 + uPort * 4;
		break;
	}

	uReg = KS8695_READ_REG(uOffset);
	/* clear corresponding bits first */
	uReg &= 0xFFFF1FFF;
	if (LINK_SELECTION_FORCED == DI.byDisableAutoNego[uPort]) {
		uReg |= SW_PORT_DISABLE_AUTONEG;
		/* force to half duplex */
		DI.bRxFlowCtrl = FALSE;
		DI.bTxFlowCtrl = FALSE;
		if (uSpeed) {
			uReg |= SW_PORT_100BASE;
			if (uDuplex) {
				uReg |= SW_PORT_FULLDUPLEX;
				DI.usCType[uPort] = SW_PHY_100BASE_TX_FD;
			}
			else {
				DI.usCType[uPort] = SW_PHY_100BASE_TX;
			}
		}
		else {
			if (uDuplex) {
				uReg |= SW_PORT_FULLDUPLEX;
				DI.usCType[uPort] = SW_PHY_10BASE_T_FD;
			}
			else {
				DI.usCType[uPort] = SW_PHY_10BASE_T;
			}
		}
	}
	else {
		if (DMA_WAN == DI.usDMAId) {
			DI.bRxFlowCtrl = FLOWCONTROL_DEFAULT;
			DI.bTxFlowCtrl = FLOWCONTROL_DEFAULT;
		}
		if (LINK_SELECTION_FULL_AUTO == DI.byDisableAutoNego[uPort]) {
			DI.usCType[uPort] = SW_PHY_AUTO;
		} else {
			switch (uSpeed) {
				case 0:
					if (uDuplex) 
						DI.usCType[uPort] = SW_PHY_10BASE_T_FD;		/* 10Base-TX Full Duplex */
					else {
						/* don't advertise flow control in half duplex case */
						if (DMA_WAN == DI.usDMAId) {
							DI.bRxFlowCtrl = FALSE;
							DI.bTxFlowCtrl = FALSE;
						}
						DI.usCType[uPort] = SW_PHY_10BASE_T;		/* 10Base-T Half Duplex */
					}
					break;

				case 1:
				default:
					if (uDuplex)
						DI.usCType[uPort] = SW_PHY_100BASE_TX_FD;	/* 100Base-TX Full Duplex */
					else {
						/* don't advertise flow control in half duplex case */
						if (DMA_WAN == DI.usDMAId) {
							DI.bRxFlowCtrl = FALSE;
							DI.bTxFlowCtrl = FALSE;
						}
						DI.usCType[uPort] = SW_PHY_100BASE_TX;		/* 100Base-TX Half Duplex */
					}
					break;
			}
		}
	}

	KS8695_WRITE_REG(uOffset, uReg);
	/* need 20 cpu clock delay for switch related registers */
	DelayInMicroseconds(10);
#ifdef	DEBUG_THIS
	DRV_INFO("%s: media type=%d, offset=0x%08x, port=%d", __FUNCTION__, DI.usCType[uPort], uOffset, uPort);
#endif

	DI.bLinkChanged[uPort] = TRUE;
	DI.bLinkActive[uPort] = FALSE;		/* watchdog routine will check link status if reset this variable!!! */
	swPhyReset(Adapter, uPort);
	swAutoNegoAdvertisement(Adapter, uPort);
	swDetectPhyConnection(Adapter, uPort);
}

/*
 * swPhyLoopback
 *	This function is used to set loopback in PHY layer.
 *
 * Argument(s)
 *	Adapter		pointer to ADAPTER_STRUCT structure.
 *  uPort		port to start
 *	bLoopback	indicates loopback enable or not
 *
 * Return(s)
 *	TRUE	if succeeded
 *	FALSE	otherwise
 */
BOOLEAN swPhyLoopback(PADAPTER_STRUCT Adapter, UINT uPort, BOOLEAN bLoopback)
{
	UINT	uReg, uOff, uShift = 0;

	switch (DI.usDMAId) {
	case DMA_HPNA:
		return FALSE;

	case DMA_WAN:
		uOff = REG_WAN_POWERMAGR;
		uShift = 0;
		break;

	default:
	case DMA_LAN:
		switch (uPort) {
		case SW_PORT_4:
			uOff = REG_LAN34_POWERMAGR;
			break;

		case SW_PORT_3:
			uOff = REG_LAN34_POWERMAGR;
			uShift = 16;
			break;

		case SW_PORT_2:
			uOff = REG_LAN12_POWERMAGR;
			break;

		case SW_PORT_1:
		default:
			uOff = REG_LAN12_POWERMAGR;
			uShift = 16;
			break;
		}
	}

	uReg = KS8695_READ_REG(uOff);
	if (bLoopback)
		uReg |= ((UINT)POWER_LOOPBACK << uShift);
	else
		uReg &= ~((UINT)POWER_LOOPBACK << uShift);
	KS8695_WRITE_REG(uOff, uReg);
	/* need 20 cpu clock delay for switch related registers */
	DelayInMicroseconds(10);

	return TRUE;
}

/*
 * swGetMacAddress
 *	This function is use to get switch engine Mac address and store it in stSwitchMac.
 *
 * Argument(s)
 *  Adapter		pionter to ADAPTER_STRUCT data structure.
 *
 * Return(s)
 *	NONE
 */
void swGetMacAddress(PADAPTER_STRUCT Adapter)
{
	UINT32 uTmp;
	int i;

#ifdef	DEBUG_THIS
	DRV_INFO("%s", __FUNCTION__);
#endif

	uTmp = KS8695_READ_REG(REG_SWITCH_MAC_LOW);
	for (i = (MAC_ADDRESS_LEN - 1); i > 1; i--)
	{
		DI.stSwitchMac[i] = (UCHAR)(uTmp & 0x0ff);
		uTmp >>= 8;
	}
	/* read high 2 bytes */
	uTmp = KS8695_READ_REG(REG_SWITCH_MAC_HIGH);
	DI.stSwitchMac[1] = (UCHAR)(uTmp & 0x0ff);
	uTmp >>= 8;
	DI.stSwitchMac[0] = (UCHAR)(uTmp & 0x0ff);
}

/*
 * swSetMacAddress
 *	This function is use to set switch engine Mac address.
 *
 * Argument(s)
 *  Adapter		pionter to ADAPTER_STRUCT data structure.
 *	pMac		pointer to mac address buffer (should be 6)
 *
 * Return(s)
 *	NONE
 */
void swSetMacAddress(PADAPTER_STRUCT Adapter, UCHAR *pMac)
{
	uint32_t uLowAddress, uHighAddress;

#ifdef	DEBUG_THIS
	DRV_INFO("%s", __FUNCTION__);
#endif

	memcpy(&DI.stSwitchMac, pMac, MAC_ADDRESS_LEN);
	uLowAddress = (*(pMac + 5)		|
				  (*(pMac + 4) << 8)	|
				  (*(pMac + 3) << 16)|
				  *(pMac + 3) << 24);

	uHighAddress = (*(pMac + 1)		|
				   *pMac << 8);

	KS8695_WRITE_REG(REG_SWITCH_MAC_LOW, uLowAddress);
	/* need 20 cpu clock delay for switch related registers */
	DelayInMicroseconds(10);
	KS8695_WRITE_REG(REG_SWITCH_MAC_HIGH, uHighAddress);
	/* need 20 cpu clock delay for switch related registers */
	DelayInMicroseconds(10);
}

/*
 * swResetSNMPInfo
 *	This function is use to get SNMP counters information
 *
 * Argument(s)
 *  Adapter		pionter to ADAPTER_STRUCT data structure.
 *
 * Return(s)
 *	NONE
 */
void swResetSNMPInfo(PADAPTER_STRUCT Adapter)
{
	memset(&Adapter->net_stats, 0, sizeof(Adapter->net_stats));
}

/*
 * swCreateLookUpTable
 *	This function is use to create loopup table.
 *
 * Argument(s)
 *  Adapter		pionter to ADAPTER_STRUCT data structure.
 *
 * Return(s)
 *	NONE
 */
void swCreateLookUpTable(PADAPTER_STRUCT Adapter)
{
	unsigned int mac = 0, index = 0, tmp = 0, portmap = 0;

#ifdef	DEBUG_THIS
	DRV_INFO("%s", __FUNCTION__);
#endif

	mac = 0x01020304;
	portmap = 0x10000;

	for (index=0; index<5; index++)
	{
		KS8695_WRITE_REG(REG_SWITCH_LUE_HIGH, 0x200000 + (portmap << index));
		DelayInMicroseconds(10);

		KS8695_WRITE_REG(REG_SWITCH_LUE_LOW, mac++);
		DelayInMicroseconds(10);

		KS8695_WRITE_REG(REG_SWITCH_LUE_CTRL, index);
		DelayInMicroseconds(10);
		
		do 
		{
			tmp = KS8695_READ_REG(REG_SWITCH_LUE_CTRL) & 0x1000; 
		} while (tmp);
	}
}

/*
 * swConfigTagRemoval
 *	This function is use to configure tag removal for ingress to given port.
 *
 * Argument(s)
 *  Adapter		pionter to ADAPTER_STRUCT data structure.
 *	uPort		port for the tag to insert
 *	bRemoval	enable/disable removal
 *
 * Return(s)
 *	NONE
 */
void swConfigTagRemoval(PADAPTER_STRUCT Adapter, UINT uPort, UINT bRemoval)
{
	uint32_t	uReg;

	uReg = KS8695_READ_REG(REG_SWITCH_ADVANCED);
	if (bRemoval) {
		uReg |= (1L << (22 + uPort));
	}
	else {
		uReg &= ~(1L << (22 + uPort));
	}
	KS8695_WRITE_REG(REG_SWITCH_ADVANCED, uReg);
	/* need 20 cpu clock delay for switch related registers */
	DelayInMicroseconds(10);
}

/*
 * swConfigTagInsertion
 *	This function is use to configure tag insertion for engress to given port.
 *
 * Argument(s)
 *  Adapter		pionter to ADAPTER_STRUCT data structure.
 *	uPort		port for the tag to insert
 *	bInsert		enable/disable insertion
 *
 * Return(s)
 *	NONE
 */
void swConfigTagInsertion(PADAPTER_STRUCT Adapter, UINT uPort, UINT bInsert)
{
	uint32_t	uReg;

	uReg = KS8695_READ_REG(REG_SWITCH_ADVANCED);
	if (bInsert) {
		uReg |= (1L << (17 + uPort));
	}
	else {
		uReg &= ~(1L << (17 + uPort));
	}
	KS8695_WRITE_REG(REG_SWITCH_ADVANCED, uReg);
	/* need 20 cpu clock delay for switch related registers */
	DelayInMicroseconds(10);
}

/*
 * gpioConfigure
 *	This function is use to configure GPIO pins required for extra LEDs
 *	as speed indicators.
 *
 * Argument(s)
 *  Adapter		pionter to ADAPTER_STRUCT data structure.
 *
 * Return(s)
 *	NONE
 */
void gpioConfigure(PADAPTER_STRUCT Adapter)
{
	uint32_t	uReg;

	uReg = KS8695_READ_REG(REG_GPIO_MODE);
	switch (DI.usDMAId) {
	case DMA_HPNA:
		return;

	case DMA_LAN:
		uReg |= 0xf0;	/* GPIO 4-7 for port 1 - 4, configure them as output */
		break;

	default:
	case DMA_WAN:
		uReg |= 0x08;	/* GPIO 3 for WAN port */
		break;
	}
	KS8695_WRITE_REG(REG_GPIO_MODE, uReg);
}

/*
 * gpioSet
 *	This function is use to set/reset given GPIO pin corresponding to the port.
 *
 * Argument(s)
 *  Adapter		pionter to ADAPTER_STRUCT data structure.
 *	uPort		port for the tag to insert
 *	bSet		enable/disable LED
 *
 * Return(s)
 *	NONE
 */
void gpioSet(PADAPTER_STRUCT Adapter, UINT uPort, UINT bSet)
{
	uint32_t	uReg;

#ifdef	DEBUG_THIS
	DRV_INFO("%s: port %d, %s", __FUNCTION__, uPort, bSet ? "100" : "10");
#endif

	uReg = KS8695_READ_REG(REG_GPIO_DATA);
	switch (DI.usDMAId) {
	case DMA_HPNA:
		return;

	case DMA_LAN:
		if (bSet)
			uReg &= ~(1 << (uPort + 4));	/* low for LED on */
		else
			uReg |= (1 << (uPort + 4));	/* high for LED off */
		break;

	default:
	case DMA_WAN:
		if (bSet)
			uReg &= ~0x08;	/* low for LED on */
		else
			uReg |= 0x08;	/* high for LED off */
		break;
	}
	KS8695_WRITE_REG(REG_GPIO_DATA, uReg);
}
