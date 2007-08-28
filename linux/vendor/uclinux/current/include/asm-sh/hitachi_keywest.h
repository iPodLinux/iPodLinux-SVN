#ifndef __ASM_SH_HITACHI_KEYWEST_H
#define __ASM_SH_HITACHI_KEYWEST_H

#ifndef CONFIG_SH_TAHOE
#define CONFIG_SH_TAHOE 1
/* for hiword interrupts of FPGA Interrupt controller */
#if 0
#define FPGA_HIWORD_IRQ            (15 - 7)
#define FPGA_HIWORD_IRQBASE 48
#endif
#if defined(CONFIG_SH_TAHOE)

/* for HD64465 INTC */
#define HD64465_IOBASE  0xb0000000
//#define HD64465_IRQBASE         64
#define HD64465_IRQBASE         80
//debug
//#define HD64465_IRQ     (15 - 3)
//#define HD64465_PCC0_IRQ (HD64465_IRQBASE + 16)
//#define HD64465_PCC1_IRQ (HD64465_IRQBASE + 17)

#if !defined(CONFIG_SH_MQ200)
#define CONFIG_SH_MQ200 1
#endif
#define MQ200_IOBASE 0xb2000000
//#define MQ200_IRQ    13
#define MQ200_IRQ    34

#endif
#if 0
#define CMDREG    0xa4040000 /* FPGA Command Register */
#define STSREG    0xa4040010 /* FPGA Status Register */
#define SWRST     0xa4040020 /* FPGA System Reset Regster */
#define SYSLED    0xa4040030 /* FPGA System LED Register */
#define PCIBMP    0xa4040040 /* FPGA PCI Base Map Address Register */
#define DATECD    0xa4040050 /* FPGA Date Code Register */
#define IMASK     0xa4040060 /* FPGA Interrupt Mask Regster */
#define ISTAT     0xa4040070 /* FPGA Interrupt Status Register */

#define IDEREG    0xa4050100 /* IDE Control Mask Register */

#define HPLED0    0xa4010060 /* FPGA HP LED Registers */
#define HPLED1    0xa4010064
#define HPLED2    0xa4010068
#define HPLED3    0xa401006C
#define HPLED4    0xa4010070
#define HPLED5    0xa4010074
#define HPLED6    0xa4010078
#define HPLED7    0xa401007C

#define PPDATA    0xa4020000
#define PPCTRL    0xa4020008

#define IDE_IOBASE  0xa4050FC0
#define IDE_01F0  (IDE_IOBASE+0)
#define IDE_01F1  (IDE_IOBASE+4)
#define IDE_01F2  (IDE_IOBASE+8)
#define IDE_01F3  (IDE_IOBASE+12)

#define IDE_01F4  (IDE_IOBASE+16)
#define IDE_01F5  (IDE_IOBASE+20)
#define IDE_01F6  (IDE_IOBASE+24)
#define IDE_01F7  (IDE_IOBASE+28)
#define IDE_03F6  0xa40507D8
#endif
//#define SMSC_BASE 0xa4400300
#define SMSC_BASE 0xb1fe0000

#define CMDREG_DBDTR   0x80000000 /* DBG-DTR of RS232 CN1 (1:on) */
#define CMDREG_DDT     0x08000000 /* DDT DMA Mode (1:on) */
#define CMDREG_RSTLAN  0x00200000 /* Reset SMSCI (1:reset) */
#define CMDREG_RSTPCI  0x00000010 /* Reset PCI (1:reset) */
#define CMDREG_FLWP    0x00000001 /* Flash write protect (1:off) */

#define STSREG_DBDST   0x00000400 /* DBG-DSR from RS232 CN1 */
#define STSREG_DBGCD   0x00000200 /* DBG-CD  from RS232 CN1 */
#define STSREG_DBGRI   0x00000100 /* DBG-RI  from RS232 CN1 */
#define STSREG_S1_MASK 0x0000000F

#define SYSLED_OFF     0x00000001
#define SYSLED_0N      0x00000000

#define PCIBMP_AC32    0x00000000 /* 32bit */
#define PCIBMP_AC16    0x00000100 /* 16bit */
#define PCIBMP_AC8     0x00001000 /* 8bit */

#define IMASK_APCI9S   0x00800000 /* AutoPCI9 */
#define IMASK_APCI8S   0x00400000 /* AutoPCI8 */
#define IMASK_APCI7S   0x00200000 /* AutoPCI7 */
#define IMASK_APCI6S   0x00100000 /* AutoPCI6 */
#define IMASK_APCI5S   0x00080000 /* AutoPCI5 */
#define IMASK_APCI4S   0x00040000 /* AutoPCI4 */
#define IMASK_APCI3S   0x00020000 /* AutoPCI3 */
#define IMASK_APCI2S   0x00010000 /* AutoPCI2 */
#define IMASK_IPCIDS   0x00008000 /* PCI Power supplu degrade  */
#define IMASK_IPCIES   0x00004000 /* PCI ENUM# */
#define IMASK_ILANS    0x00002000 /* SMSC Ethernet */
#define IMASK_IV3S     0x00000800 /* V3 */
#define IMASK_IV3TS    0x00000400 /* V3 timeout */
#define IMASK_IPCISS   0x00000200 /* PCI party error */
#define IMASK_APCIS    0x00000080 /* AutoPCI */
#define IMASK_IIDES    0x00000020 /* IDE */
#define IMASK_IIOS     0x00000010 /* Expansion IO */
#define IMASK_IRS65S   0x00000008 /* HD64465 */
#define IMASK_IRS64S   0x00000004 /* HD64464 LCD/CRT */
#define IMASK_IRS2S    0x00000002 /* Ring Indicator from RS232 CN1 */

#define ISTAT_APCI9S   0x00800000
#define ISTAT_APCI8S   0x00400000
#define ISTAT_APCI7S   0x00200000
#define ISTAT_APCI6S   0x00100000
#define ISTAT_APCI5S   0x00080000
#define ISTAT_APCI4S   0x00040000
#define ISTAT_APCI3S   0x00020000
#define ISTAT_APCI2S   0x00010000
#define ISTAT_IPCIDS   0x00008000
#define ISTAT_IPCIES   0x00004000
#define ISTAT_ILANS    0x00002000
#define ISTAT_IV3S     0x00000800
#define ISTAT_IV3TS    0x00000400
#define ISTAT_IPCISS   0x00000200
#define ISTAT_APCIS    0x00000080
#define ISTAT_IIDES    0x00000020
#define ISTAT_IIOS     0x00000010
#define ISTAT_IRS65S   0x00000008
#define ISTAT_IRS64S   0x00000004
#define ISTAT_IRS2S    0x00000002

//#if defined(__KERNEL__)
//void SysLED(int);
//void HpLED_set(int, int);
//void HpLED_puts(const char*);
//void HpLED_printf(const char*, ...);
//#endif

#endif

#endif
