/*
 * xilinx_syms.c
 *
 * This file EXPORT_SYMBOL's all of the Xilinx entry points.
 *
 * Author: MontaVista Software, Inc.
 *         source@mvista.com
 *
 * 2002 (c) MontaVista, Software, Inc.  This file is licensed under the terms
 * of the GNU General Public License version 2.  This program is licensed
 * "as is" without any warranty of any kind, whether express or implied.
 */

#include <linux/module.h>

#include "xbasic_types.h"
EXPORT_SYMBOL(XAssert);
EXPORT_SYMBOL(XAssertSetCallback);
EXPORT_SYMBOL(XAssertStatus);
extern u32 XWaitInAssert;
EXPORT_SYMBOL(XWaitInAssert);

#include "xdma_channel.h"
EXPORT_SYMBOL(XDmaChannel_CommitPuts);
EXPORT_SYMBOL(XDmaChannel_CreateSgList);
EXPORT_SYMBOL(XDmaChannel_DecrementPktCount);
EXPORT_SYMBOL(XDmaChannel_GetControl);
EXPORT_SYMBOL(XDmaChannel_GetDescriptor);
EXPORT_SYMBOL(XDmaChannel_GetIntrEnable);
EXPORT_SYMBOL(XDmaChannel_GetIntrStatus);
EXPORT_SYMBOL(XDmaChannel_GetPktCount);
EXPORT_SYMBOL(XDmaChannel_GetPktThreshold);
EXPORT_SYMBOL(XDmaChannel_GetPktWaitBound);
EXPORT_SYMBOL(XDmaChannel_GetStatus);
EXPORT_SYMBOL(XDmaChannel_GetVersion);
EXPORT_SYMBOL(XDmaChannel_Initialize);
EXPORT_SYMBOL(XDmaChannel_IsReady);
EXPORT_SYMBOL(XDmaChannel_IsSgListEmpty);
EXPORT_SYMBOL(XDmaChannel_PutDescriptor);
EXPORT_SYMBOL(XDmaChannel_Reset);
EXPORT_SYMBOL(XDmaChannel_SelfTest);
EXPORT_SYMBOL(XDmaChannel_SetControl);
EXPORT_SYMBOL(XDmaChannel_SetIntrEnable);
EXPORT_SYMBOL(XDmaChannel_SetIntrStatus);
EXPORT_SYMBOL(XDmaChannel_SetPktThreshold);
EXPORT_SYMBOL(XDmaChannel_SetPktWaitBound);
EXPORT_SYMBOL(XDmaChannel_SgStart);
EXPORT_SYMBOL(XDmaChannel_SgStop);
EXPORT_SYMBOL(XDmaChannel_Transfer);

#include "xipif_v1_23_b.h"
EXPORT_SYMBOL(XIpIfV123b_SelfTest);

#include "xpacket_fifo_v1_00_b.h"
EXPORT_SYMBOL(XPacketFifoV100b_Initialize);
EXPORT_SYMBOL(XPacketFifoV100b_Read);
EXPORT_SYMBOL(XPacketFifoV100b_SelfTest);
EXPORT_SYMBOL(XPacketFifoV100b_Write);

#include "xversion.h"
EXPORT_SYMBOL(XVersion_Copy);
EXPORT_SYMBOL(XVersion_FromString);
EXPORT_SYMBOL(XVersion_IsEqual);
EXPORT_SYMBOL(XVersion_Pack);
EXPORT_SYMBOL(XVersion_ToString);
EXPORT_SYMBOL(XVersion_UnPack);
