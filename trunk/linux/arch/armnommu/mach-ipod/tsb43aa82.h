
/*
 * Copyright (c) 2004 Bernard Leach (leachbj@bouncycastle.org)
 */
 
#ifndef __TSB43AA82_H__
#define __TSB43AA82_H__

/**** Lynx internal register addresses ****/

#define LYNX_VERSION          0x00
#define LYNX_ACK              0x04
#define LYNX_CTRL             0x08
#define LYNX_INTERRUPT        0x0C
#define LYNX_IMASK            0x10
#define LYNX_CYCLE_TIMER      0x14
#define LYNX_DIAGNOSTIC       0x18
#define LYNX_PHYACCESS        0x20
#define LYNX_BUS_RESET        0x24
#define LYNX_TIMELIMIT        0x28
#define LYNX_ATF_STATUS       0x2C
#define LYNX_ARF_STATUS       0x30
#define LYNX_MTQ_STATUS       0x34
#define LYNX_MRF_STATUS       0x38
#define LYNX_CTQ_STATUS       0x3C
#define LYNX_CRF_STATUS       0x40
#define LYNX_ORB_FETCH_CTRL   0x44
#define LYNX_MANAGEMENT_AGENT 0x48
#define LYNX_COMMAND_AGENT    0x4C
#define LYNX_AGENT_CTRL       0x50
#define LYNX_ORB_PTR1         0x54
#define LYNX_ORB_PTR2         0x58
#define LYNX_AGENT_STATUS     0x5C
#define LYNX_TXTIMER_CTRL     0x60
#define LYNX_TXTIMER_STATUS1  0x64
#define LYNX_TXTIMER_STATUS2  0x68
#define LYNX_TXTIMER_STATUS3  0x6C
#define LYNX_WRITE_FIRST      0x70
#define LYNX_WRITE_CONTINUE   0x74
#define LYNX_WRITE_UPDATE     0x78

#define LYNX_ARF_DATA         0x80
#define LYNX_MRF_DATA         0x84
#define LYNX_CRF_DATA         0x88
#define LYNX_CFR_CTRL         0x8C
#define LYNX_DMA_CTRL         0x90
#define LYNX_BI_CTRL          0x94
#define LYNX_DXF_SIZE         0x98
#define LYNX_DXF_AVAIL        0x9C
#define LYNX_DXF_ACK          0xA0
#define LYNX_DTF_1ST_CONTINUE 0xA4
#define LYNX_DTF_UPDATE       0xA8
#define LYNX_DRF_DATA         0xAC
#define LYNX_DTF_CTRL0        0xB0
#define LYNX_DTF_CTRL1        0xB4
#define LYNX_DTF_CTRL2        0xB8
#define LYNX_DTF_CTRL3        0xBC
#define LYNX_DRF_CTRL0        0xC0
#define LYNX_DRF_CTRL1        0xC4
#define LYNX_DRF_CTRL2        0xC8
#define LYNX_DRF_CTRL3        0xCC
#define LYNX_DRF_HDR0         0xD0
#define LYNX_DRF_HDR1         0xD4
#define LYNX_DRF_HDR2         0xD8
#define LYNX_DRF_HDR3         0xDC
#define LYNX_DRF_TAILER       0xE0
#define LYNX_DXF_EXPCTD_VALUE 0xE4
#define LYNX_DXF_HDRSTAT0     0xE8
#define LYNX_DXF_HDRSTAT1     0xEC
#define LYNX_DXF_HDRSTAT2     0xF0
#define LYNX_DXF_HDRSTAT3     0xF4
#define LYNX_LOG_ROM_CTRL     0xF8
#define LYNX_LOG_ROM_DATA     0xFC

/************************************************************************/
/*      PHY register offsets for 43AA82                          */
/************************************************************************/
#define PHY_ID_R_CPS        0x00
#define PHY_RHB_IBR_GC      0x01
#define PHY_NUMPORTS        0x02
#define PHY_SPD_DELAY       0x03
#define PHY_LC_C_JITTER_PWR 0x04
#define PHY_RPIE_ETC        0x05
#define PHY_PAGE_PORT       0x07



/************************************************************************/
/*      bit definition for LYNX_VERSION (0x00)                          */
/************************************************************************/
#define LYNX_VERSION_CHIP_ID      0x43008203ul
#define LYNX_VERSION_BIG_ENDIAN   0xFFFFFFFFul

/************************************************************************/
/*      bit definition for LYNX_CTRL(0x08)                              */
/************************************************************************/
#define LYNX_CTRL_ID_VALID            0x80000000ul
#define LYNX_CTRL_SELFID_RX_ENABLE    0x40000000ul
#define LYNX_CTRL_SELFID_TO_DRF       0x20000000ul
#define LYNX_CTRL_ACK_BUSY_X          0x08000000ul
#define LYNX_CTRL_TRANS_ENABLE        0x04000000ul
#define LYNX_CTRL_ACCELARATED_ARB     0x01000000ul
#define LYNX_CTRL_CONCAT_ENABLE       0x00800000ul
#define LYNX_CTRL_RESET_TRANS         0x00200000ul
#define LYNX_CTRL_SEND_RESP_ERROR     0x00020000ul
#define LYNX_CTRL_STORE_ERROR_PKT     0x00010000ul
#define LYNX_CTRL_SPLIT_TRANS_ENABLE  0x00008000ul
#define LYNX_CTRL_RETRY_ENABLE        0x00004000ul
#define LYNX_CTRL_ACK_PENDING         0x00002000ul
#define LYNX_CTRL_CYCLE_MASTER        0x00000800ul
#define LYNX_CTRL_CYCLE_TIMER_ENABLE  0x00000200ul
#define LYNX_CTRL_DMA_CLEAR           0x00000100ul
#define LYNX_CTRL_RCV_UNEXPECT_PKT    0x00000080ul
#define LYNX_CTRL_UNEXPECT_PKT_TO_DRF 0x00000040ul

/************************************************************************/
/*      bit definition for LYNX_INTERRUPT(0x0C), LYNX_IMASK(0x10)       */
/************************************************************************/
#define LYNX_INTERRUPT_ALL_CLEAR  0xFFFFFFFFul
#define LYNX_INTERRUPT_Int        0x80000000ul
#define LYNX_INTERRUPT_PhyInt     0x40000000ul
#define LYNX_INTERRUPT_BusRst     0x20000000ul
#define LYNX_INTERRUPT_CmdRst     0x10000000ul
#define LYNX_INTERRUPT_SelfIDEnd  0x08000000ul
#define LYNX_INTERRUPT_PhyPkt     0x04000000ul
#define LYNX_INTERRUPT_SntRj      0x00800000ul
#define LYNX_INTERRUPT_RxPhyReg   0x00400000ul
#define LYNX_INTERRUPT_InvFAcc    0x00200000ul
#define LYNX_INTERRUPT_HdErr      0x00100000ul
#define LYNX_INTERRUPT_TCErr      0x00080000ul
#define LYNX_INTERRUPT_CySec      0x00040000ul
#define LYNX_INTERRUPT_CyStart    0x00020000ul
#define LYNX_INTERRUPT_UpdDRFHdr  0x00008000ul
#define LYNX_INTERRUPT_FairGap    0x00004000ul
#define LYNX_INTERRUPT_TxRdy      0x00002000ul
#define LYNX_INTERRUPT_CyDne      0x00001000ul
#define LYNX_INTERRUPT_CyPending  0x00000800ul
#define LYNX_INTERRUPT_CyLost     0x00000400ul
#define LYNX_INTERRUPT_CyArbFail  0x00000200ul
#define LYNX_INTERRUPT_ATFEnd     0x00000080ul
#define LYNX_INTERRUPT_RxARF      0x00000040ul
#define LYNX_INTERRUPT_MgtORBEnd  0x00000020ul
#define LYNX_INTERRUPT_CmdORBEnd  0x00000010ul
#define LYNX_INTERRUPT_DTFEnd     0x00000008ul
#define LYNX_INTERRUPT_DRFEnd     0x00000004ul
#define LYNX_INTERRUPT_TxExpr     0x00000002ul
#define LYNX_INTERRUPT_AgntWr     0x00000001ul


/************************************************************************/
/*      bit definition for LYNX_PHYACCESS (0x20)                        */
/************************************************************************/
#define LYNX_PHYACCESS_BUS_RESET  0x417F0000ul

/* Bits for Bus Reset register :  */
/************************************************************************/
/*      bit definition for LYNX_BUS_RESET  (0x24)                       */
/************************************************************************/
#define LYNX_BUSRESET_BUSNUMBER   0xFFC00000ul
#define LYNX_BUSRESET_NODENUM     0x003F0000ul
#define LYNX_BUSRESET_ERRCODE     0x0000F000ul
#define LYNX_BUSRESET_NODESUM     0x00000FC0ul
#define LYNX_BUSRESET_IRMNODE     0x0000003Ful

/************************************************************************/
/*      bit definition for LYNX_TIMELIMIT (0x28)                        */
/************************************************************************/
//#define LYNX_TIMELIMIT_INIT_VALUE  0x3E8008E0ul /* SplTO Intrvl RtryLmt */
                                                /* 2secs  1msec    14   */
//#define LYNX_TIMELIMIT_INIT_VALUE  0x032008E0ul /* SplTO Intrvl   RtryLmt */
                                                /* 0secs  100msec    14   */

#define LYNX_TIMELIMIT_INIT_VALUE  0x0FA008E0ul /* SplTO Intrvl   RtryLmt */
                                                /* 1secs    0msec    14   */

/************************************************************************/
/*      bit definition for LYNX_XXX_STATUS(0x2C-0x40)                   */
/************************************************************************/
#define LYNX_XXFSTAT_FULL     0x80000000ul  //FIFO FULL
#define LYNX_XXFSTAT_ALMFULL  0x40000000ul  //FIFO ALMOST FULL
#define LYNX_XXFSTAT_ALMEMPTY 0x20000000ul  //FIFO ALMOST EMPTY
#define LYNX_XXFSTAT_EMPTY    0x10000000ul  //FIFO EMPTY

#define LYNX_XXFSTAT_CLEAR    0x00001000ul  //FIFO CLEAR
#define LYNX_XRFSTAT_PKT_CD   0x00008000ul  //


/************************************************************************/
/*      bit definition for LYNX_ORB_FETCH_CTRL(0x44)                    */
/************************************************************************/
#define LYNX_ORB_FETC_MGT_VALID       0x80000000ul
#define LYNX_ORB_FETC_MGT_BUSY        0x40000000ul
#define LYNX_ORB_FETC_MGT_SHORT_FMT   0x10000000ul
#define LYNX_ORB_FETC_MGT_ORB_PRIO_0  0x00000000ul
#define LYNX_ORB_FETC_MGT_ORB_PRIO_1  0x01000000ul
#define LYNX_ORB_FETC_MGT_ORB_PRIO_2  0x02000000ul
#define LYNX_ORB_FETC_MGT_ORB_PRIO_3  0x03000000ul
#define LYNX_ORB_FETC_MGT_ORB_PRIO_4  0x04000000ul
#define LYNX_ORB_FETC_MGT_ORB_PRIO_5  0x05000000ul
#define LYNX_ORB_FETC_MGT_ORB_PRIO_6  0x06000000ul
#define LYNX_ORB_FETC_MGT_ORB_PRIO_7  0x07000000ul
#define LYNX_ORB_FETC_MGT_ORB_PRIO_8  0x08000000ul
#define LYNX_ORB_FETC_MGT_ORB_PRIO_9  0x09000000ul
#define LYNX_ORB_FETC_MGT_ORB_PRIO_A  0x0A000000ul
#define LYNX_ORB_FETC_MGT_ORB_PRIO_B  0x0B000000ul
#define LYNX_ORB_FETC_MGT_ORB_PRIO_C  0x0C000000ul
#define LYNX_ORB_FETC_MGT_ORB_PRIO_D  0x0D000000ul
#define LYNX_ORB_FETC_MGT_ORB_PRIO_E  0x0E000000ul
#define LYNX_ORB_FETC_MGT_ORB_PRIO_F  0x0F000000ul
#define LYNX_ORB_FETC_CMD_AGENT0_VLD  0x00008000ul
#define LYNX_ORB_FETC_CMD_AGENT1_VLD  0x00004000ul
#define LYNX_ORB_FETC_CMD_AGENT2_VLD  0x00002000ul
#define LYNX_ORB_FETC_CMD_AGENT3_VLD  0x00001000ul
#define LYNX_ORB_FETC_DRBELL_SNP      0x00000800ul
#define LYNX_ORB_FETC_DRBELL_FETCH    0x00000400ul
#define LYNX_ORB_FETC_NEXT_CMD_FETCH  0x00000200ul
#define LYNX_ORB_FETC_CMD_SHORT_FMT   0x00000100ul
#define LYNX_ORB_FETC_CMD_AGENT0_RDY  0x00000080ul
#define LYNX_ORB_FETC_CMD_AGENT1_RDY  0x00000040ul
#define LYNX_ORB_FETC_CMD_AGENT2_RDY  0x00000020ul
#define LYNX_ORB_FETC_CMD_AGENT3_RDY  0x00000010ul
#define LYNX_ORB_FETC_CMD_ORB_PRIO_0  0x00000000ul
#define LYNX_ORB_FETC_CMD_ORB_PRIO_1  0x00000001ul
#define LYNX_ORB_FETC_CMD_ORB_PRIO_2  0x00000002ul
#define LYNX_ORB_FETC_CMD_ORB_PRIO_3  0x00000003ul
#define LYNX_ORB_FETC_CMD_ORB_PRIO_4  0x00000004ul
#define LYNX_ORB_FETC_CMD_ORB_PRIO_5  0x00000005ul
#define LYNX_ORB_FETC_CMD_ORB_PRIO_6  0x00000006ul
#define LYNX_ORB_FETC_CMD_ORB_PRIO_7  0x00000007ul
#define LYNX_ORB_FETC_CMD_ORB_PRIO_8  0x00000008ul
#define LYNX_ORB_FETC_CMD_ORB_PRIO_9  0x00000009ul
#define LYNX_ORB_FETC_CMD_ORB_PRIO_A  0x0000000Aul
#define LYNX_ORB_FETC_CMD_ORB_PRIO_B  0x0000000Bul
#define LYNX_ORB_FETC_CMD_ORB_PRIO_C  0x0000000Cul
#define LYNX_ORB_FETC_CMD_ORB_PRIO_D  0x0000000Dul
#define LYNX_ORB_FETC_CMD_ORB_PRIO_E  0x0000000Eul
#define LYNX_ORB_FETC_CMD_ORB_PRIO_F  0x0000000Ful


/************************************************************************/
/*      bit definition for LYNX_AGENT_CTRL (0x50)                       */
/************************************************************************/
#define LYNX_AGENTCTRL_USTLEN   0x00100000ul
#define LYNX_AGENTCTRL_VALID    0x00080000ul
#define LYNX_AGENTCTRL_WR_ID    0x00020000ul
#define LYNX_AGENTCTRL_RD_ID    0x00010000ul

/************************************************************************/
/*      bit definition for LYNX_TXTIMER_CTRL (0x60)                     */
/************************************************************************/
#define LYNX_TXTIMER_DTTXEND    0x80000000ul
#define LYNX_TXTIMER_DRTXEND    0x40000000ul
#define LYNX_TXTIMER_ATTXEND    0x20000000ul
#define LYNX_TXTIMER_MTTXEND    0x10000000ul
#define LYNX_TXTIMER_CTTXEND    0x08000000ul
#define LYNX_TXTIMER_ARTXEND    0x02000000ul
#define LYNX_TXTIMER_DTERR      0x00800000ul
#define LYNX_TXTIMER_DRERR      0x00400000ul
#define LYNX_TXTIMER_ATERR      0x00200000ul
#define LYNX_TXTIMER_MTERR      0x00100000ul
#define LYNX_TXTIMER_CTERR      0x00080000ul
#define LYNX_TXTIMER_ARERR      0x00020000ul

/************************************************************************/
/*      bit definition for LYNX_DMA_CTRL(0x90)                          */
/************************************************************************/
#define DMA_CTRL_DMA_WRITE_ENABLE  0x80000000ul // bit 0
#define DMA_CTRL_DRF_ENABLE        0x20000000ul // bit 2
#define DMA_CTRL_DTF_ENABLE        0x10000000ul // bit 3
#define DMA_CTRL_DRF_PKTZ_ENABLE   0x08000000ul // bit 4
#define DMA_CTRL_DTF_PKTZ_ENABLE   0x04000000ul // bit 5
#define DMA_CTRL_DRF_SPLIT_DISABLE 0x02000000ul // bit 6
#define DMA_CTRL_DTF_SPLIT_DISABLE 0x01000000ul // bit 7
#define DMA_CTRL_HEADER_SEL_0      0x00000000ul // bit 8:9
#define DMA_CTRL_HEADER_SEL_1      0x00400000ul // bit 8:9
#define DMA_CTRL_HEADER_SEL_2      0x00800000ul // bit 8:9
#define DMA_CTRL_HEADER_SEL_3      0x00C00000ul // bit 8:9
#define DMA_CTRL_CONF_SINGL_PKT    0x00200000ul // bit 10
#define DMA_CTRL_LONG_BLOCK        0x00100000ul // bit 11
#define DMA_CTRL_QUAD_SEND         0x00080000ul // bit 12
#define DMA_CTRL_QUAD_BOUNDARY     0x00040000ul // bit 13
#define DMA_CTRL_CHECK_PAGE_TABLE  0x00020000ul // bit 14
#define DMA_CTRL_AUTO_PAGING       0x00010000ul // bit 15

//How many PageTable ENTRIES to be fetched, must be Power of 2
#define DMA_CTRL_DRF_PAGE_FETCH_01  0x00000000ul // bit 16:18
#define DMA_CTRL_DRF_PAGE_FETCH_02  0x00002000ul // bit 16:18
#define DMA_CTRL_DRF_PAGE_FETCH_04  0x00004000ul // bit 16:18
#define DMA_CTRL_DRF_PAGE_FETCH_08  0x00006000ul // bit 16:18
#define DMA_CTRL_DRF_PAGE_FETCH_16  0x00008000ul // bit 16:18
#define DMA_CTRL_DRF_PAGE_FETCH_32  0x0000A000ul // bit 16:18
#define DMA_CTRL_DRF_PAGE_FETCH_64  0x0000C000ul // bit 16:18
#define DMA_CTRL_DRF_PAGE_FETCH_128 0x0000E000ul // bit 16:18

//How many PageTable ENTRIES to be fetched, must be Power of 2
#define DMA_CTRL_DTF_PAGE_FETCH_01  0x00000000ul // bit 19:21
#define DMA_CTRL_DTF_PAGE_FETCH_02  0x00000400ul // bit 19:21
#define DMA_CTRL_DTF_PAGE_FETCH_04  0x00000800ul // bit 19:21
#define DMA_CTRL_DTF_PAGE_FETCH_08  0x00000C00ul // bit 19:21
#define DMA_CTRL_DTF_PAGE_FETCH_16  0x00001000ul // bit 19:21
#define DMA_CTRL_DTF_PAGE_FETCH_32  0x00001400ul // bit 19:21
#define DMA_CTRL_DTF_PAGE_FETCH_64  0x00001800ul // bit 19:21
#define DMA_CTRL_DTF_PAGE_FETCH_128 0x00001C00ul // bit 19:21

#define DMA_CTRL_ACK_PENDING       0x00000200ul // bit 22
#define DMA_CTRL_COMPLETE_RESP     0x00000100ul // bit 23
#define DMA_CTRL_DTF_HEADER_INSERT 0x00000080ul // bit 24
#define DMA_CTRL_DRF_PAUSE         0x00000040ul // bit 25
#define DMA_CTRL_DRF_AUTOPAUSE     0x00000020ul // bit 26
#define DMA_CTRL_DRF_HEADER_STRIP  0x00000010ul // bit 27
#define DMA_CTRL_DRD_SELECT        0x00000008ul // bit 28
#define DMA_CTRL_DTD_SELECT        0x00000004ul // bit 29
#define DMA_CTRL_DRF_CLEAR         0x00000002ul // bit 30
#define DMA_CTRL_DTF_CLEAR         0x00000001ul // bit 31


/************************************************************************/
/*      bit definition for LYNX_BI_CTRL(0x94)                           */
/************************************************************************/

#define BDI_CTRL_MTR_BUF_SIZE_0   0x00000000ul // bit 3:5
#define BDI_CTRL_MTR_BUF_SIZE_2   0x04000000ul // bit 3:5
#define BDI_CTRL_MTR_BUF_SIZE_4   0x08000000ul // bit 3:5
#define BDI_CTRL_MTR_BUF_SIZE_6   0x0C000000ul // bit 3:5
#define BDI_CTRL_MTR_BUF_SIZE_8   0x10000000ul // bit 3:5
#define BDI_CTRL_MTR_BUF_SIZE_10  0x14000000ul // bit 3:5
#define BDI_CTRL_MTR_BUF_SIZE_12  0x18000000ul // bit 3:5
#define BDI_CTRL_MTR_BUF_SIZE_14  0x1C000000ul // bit 3:5

#define BDI_CTRL_MTT_BUF_SIZE_0   0x00000000ul // bit 6:8
#define BDI_CTRL_MTT_BUF_SIZE_2   0x00800000ul // bit 6:8
#define BDI_CTRL_MTT_BUF_SIZE_4   0x01000000ul // bit 6:8
#define BDI_CTRL_MTT_BUF_SIZE_6   0x01800000ul // bit 6:8
#define BDI_CTRL_MTT_BUF_SIZE_8   0x02000000ul // bit 6:8
#define BDI_CTRL_MTT_BUF_SIZE_10  0x02800000ul // bit 6:8
#define BDI_CTRL_MTT_BUF_SIZE_12  0x03000000ul // bit 6:8
#define BDI_CTRL_MTT_BUF_SIZE_14  0x03800000ul // bit 6:8

#define BDI_CTRL_BDACK_HI_ENABLE     0x00400000ul // bit 9
#define BDI_CTRL_ATACK_HI_ENABLE     0x00100000ul // bit 11
#define BDI_CTRL_BDI_BUSY_HI_ENABLE  0x00080000ul // bit 12
#define BDI_CTRL_BDO_AVAIL_HI_ENABLE 0x00040000ul // bit 13
#define BDI_CTRL_BDOEN_HI_ENABLE     0x00020000ul // bit 14
#define BDI_CTRL_BDIEN_HI_ENABLE     0x00010000ul // bit 15
#define BDI_CTRL_LITTLE_ENDIAN       0x00008000ul // bit 16
#define BDI_CTRL_AUTO_PAD_ENABL      0x00004000ul // bit 17

#define BDI_CTRL_BD0_DELAY_0  0x00000000ul // bit 18:19
#define BDI_CTRL_BD0_DELAY_1  0x00001000ul // bit 18:19
#define BDI_CTRL_BD0_DELAY_2  0x00002000ul // bit 18:19
#define BDI_CTRL_BD0_DELAY_3  0x00003000ul // bit 18:19
#define BDI_CTRL_BDI_DELAY_0  0x00000000ul // bit 20:21
#define BDI_CTRL_BDI_DELAY_1  0x00000400ul // bit 20:21
#define BDI_CTRL_BDI_DELAY_2  0x00000800ul // bit 20:21
#define BDI_CTRL_BDI_DELAY_3  0x00000C00ul // bit 20:21

#define BDI_CTRL_DMA_MODE_MASK 0x370 // (bit 22:23,25:27) mask for DMA mode bits
#define BDI_CTRL_DMA_MODE_A  0      // sync 8bit parallel in/out
#define BDI_CTRL_DMA_MODE_B  0x110  // async 8bit parallel in/out
#define BDI_CTRL_DMA_MODE_C  0x120  // async 8bit bi-directional
#define BDI_CTRL_DMA_MODE_D  0x230  // sync 8bit bi-directional
#define BDI_CTRL_DMA_MODE_E  0x340  // async 8bit bi-directional (scsi)
#define BDI_CTRL_DMA_MODE_F  0x150  // async 16bit bi-directional
#define BDI_CTRL_DMA_MODE_G  0x260  // sync 16bit bi-directional
#define BDI_CTRL_DMA_MODE_H  0x370  // async 16bit bi-directional (scsi)

#define BDI_CTRL_BURST_ENABLE   0x00000080ul // bit 24
#define BDI_CTRL_RCV_PAD_ENABLE 0x00000008ul // bit 28
#define BDI_CTRL_BDO_RESET      0x00000004ul // bit 29
#define BDI_CTRL_BDI_RESET      0x00000002ul // bit 30
#define BDI_CTRL_BD0_DISABLE    0x00000001ul // bit 31

#define BDI_SIGNALS_ALLOWED  0x007FFC89ul

/************************************************************************
 *  bit definition for LYNX_DTFDRF_SIZE(0x98)
 *
 *  These reflect the values set in the DMA Control (0x90)
 *  for Page table Fetch Size
 *
 *  (0x90) FetSize is # of PgTbl Entries to get (ie 2^((FetSize-1)+3) BYTES)
 ************************************************************************/
#define LYNX_DTFDRF_PTBUF_SIZE(NumEntries)  (2 * NumEntries)  //In Quadlets
/*
#define LYNX_DTFDRF_PTBUF_SIZE_1 0x0800  // 1 PgTbl entries (  8 bytes)
#define LYNX_DTFDRF_PTBUF_SIZE_2 0x1000  // 2 PgTbl entries ( 16 bytes)
#define LYNX_DTFDRF_PTBUF_SIZE_3 0x1800  // 3 PgTbl entries ( 24 bytes)
#define LYNX_DTFDRF_PTBUF_SIZE_4 0x2000  // 4 PgTbl entries ( 32 bytes)
#define LYNX_DTFDRF_PTBUF_SIZE_5 0x2800  // 5 PgTbl entries ( 40 bytes)
#define LYNX_DTFDRF_PTBUF_SIZE_6 0x3000  // 6 PgTbl entries ( 48 bytes)
#define LYNX_DTFDRF_PTBUF_SIZE_7 0x3800  // 7 PgTbl entries ( 56 bytes)
*/

/************************************************************************/
/*  bit definition for LYNX_DXF_CTRL0 (0xB0 & 0xC0)                     */
/************************************************************************/
#define LYNX_DXFC0_START      0x80000000ul
#define LYNX_DXFC0_INITSTART  0xC0000000ul
#define LYNX_DXFC0_ABORT      0x40000000ul
#define LYNX_DXFC0_DXFCLR     0x20000000ul


/************************************************************************/
/*      bit definition for LYNX_LOG_ROM_CTRL (0xF8)                     */
/************************************************************************/
#define LYNX_ROM_CTRL_XLOG      0x00008000ul
#define LYNX_ROM_CTRL_VALID     0x00004000ul
#define LYNX_ROM_CTRL_ROM_ADDR  0x00000400ul

#endif

