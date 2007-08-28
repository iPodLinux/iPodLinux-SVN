/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/init.c,v 1.3 2002/24/04 01:16:16 dawes Exp $ */
/*
 * Mode switching code (CRT1 section) for
 * SiS 300/540/630/730/315/550/650/M650/651/M652/740/330/660/M660/760
 * (Universal module for Linux kernel framebuffer and XFree86 4.x)
 *
 * Assembler-To-C translation
 * Copyright 2002, 2003 by Thomas Winischhofer <thomas@winischhofer.net>
 * Formerly based on non-functional code-fragements by SiS, Inc.
 *
 * If distributed as part of the linux kernel, the contents of this file
 * is entirely covered by the GPL.
 *
 * Otherwise, the following terms apply:
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holder not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The copyright holder makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * TW says: This code looks awful, I know. But please don't do anything about
 * this otherwise debugging will be hell.
 * The code is extremely fragile as regards the different chipsets, different
 * video bridges and combinations thereof. If anything is changed, extreme
 * care has to be taken that that change doesn't break it for other chipsets,
 * bridges or combinations thereof.
 * All comments in this file are by me, regardless if they are marked TW or not.
 *
 */
 
#include "init.h"

#ifdef SIS300
#include "300vtbl.h"
#endif

#ifdef SIS315H
#include "310vtbl.h"
#endif

#ifdef LINUX_XF86
BOOLEAN SiSBIOSSetMode(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension,
                       ScrnInfoPtr pScrn, DisplayModePtr mode, BOOLEAN IsCustom);
DisplayModePtr SiSBuildBuiltInModeList(ScrnInfoPtr pScrn, BOOLEAN includelcdmodes, BOOLEAN isfordvi);
#ifdef SISDUALHEAD
BOOLEAN SiSBIOSSetModeCRT1(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension,
                       ScrnInfoPtr pScrn, DisplayModePtr mode, BOOLEAN IsCustom);
BOOLEAN SiSBIOSSetModeCRT2(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension,
                       ScrnInfoPtr pScrn, DisplayModePtr mode, BOOLEAN IsCustom);
#endif /* dual head */
#endif /* linux_xf86 */

#ifdef LINUX_XF86
BOOLEAN SiSSetMode(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension,
                   ScrnInfoPtr pScrn,USHORT ModeNo, BOOLEAN dosetpitch);
#else
BOOLEAN SiSSetMode(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension,
                   USHORT ModeNo);
#endif

#ifndef LINUX_XF86
static ULONG GetDRAMSize(SiS_Private *SiS_Pr,
                         PSIS_HW_DEVICE_INFO HwDeviceExtension);
#endif

#if defined(ALLOC_PRAGMA)
#pragma alloc_text(PAGE,SiSSetMode)
#pragma alloc_text(PAGE,SiSInit)
#endif

static void
InitCommonPointer(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
   SiS_Pr->SiS_StResInfo     = SiS_StResInfo;
   SiS_Pr->SiS_ModeResInfo   = SiS_ModeResInfo;
   SiS_Pr->SiS_StandTable    = SiS_StandTable;
   if(HwDeviceExtension->jChipType < SIS_315H) {
      SiS_StandTable[0x04].CRTC[4] = 0x2b;
      SiS_StandTable[0x05].CRTC[4] = 0x2b;
      SiS_StandTable[0x06].CRTC[4] = 0x54;
      SiS_StandTable[0x06].CRTC[5] = 0x80;
      SiS_StandTable[0x0d].CRTC[4] = 0x2b;
      SiS_StandTable[0x0e].CRTC[4] = 0x54;
      SiS_StandTable[0x0e].CRTC[5] = 0x80;
      SiS_StandTable[0x11].CRTC[4] = 0x54;
      SiS_StandTable[0x11].CRTC[5] = 0x80;
      SiS_StandTable[0x11].CRTC[16] = 0x83;
      SiS_StandTable[0x11].CRTC[17] = 0x85;
      SiS_StandTable[0x12].CRTC[4] = 0x54;
      SiS_StandTable[0x12].CRTC[5] = 0x80;
      SiS_StandTable[0x12].CRTC[16] = 0x83;
      SiS_StandTable[0x12].CRTC[17] = 0x85;
      SiS_StandTable[0x13].CRTC[5] = 0xa0;
      SiS_StandTable[0x17].CRTC[5] = 0xa0;
      SiS_StandTable[0x1a].CRTC[4] = 0x54;
      SiS_StandTable[0x1a].CRTC[5] = 0x80;
      SiS_StandTable[0x1a].CRTC[16] = 0xea;
      SiS_StandTable[0x1a].CRTC[17] = 0x8c;
      SiS_StandTable[0x1b].CRTC[4] = 0x54;
      SiS_StandTable[0x1b].CRTC[5] = 0x80;
      SiS_StandTable[0x1b].CRTC[16] = 0xea;
      SiS_StandTable[0x1b].CRTC[17] = 0x8c;
      SiS_StandTable[0x1c].CRTC[4] = 0x54;
      SiS_StandTable[0x1c].CRTC[5] = 0x80;
   } else {
      SiS_StandTable[0x04].CRTC[4] = 0x2c;
      SiS_StandTable[0x05].CRTC[4] = 0x2c;
      SiS_StandTable[0x06].CRTC[4] = 0x55;
      SiS_StandTable[0x06].CRTC[5] = 0x81;
      SiS_StandTable[0x0d].CRTC[4] = 0x2c;
      SiS_StandTable[0x0e].CRTC[4] = 0x55;
      SiS_StandTable[0x0e].CRTC[5] = 0x81;
      SiS_StandTable[0x11].CRTC[4] = 0x55;
      SiS_StandTable[0x11].CRTC[5] = 0x81;
      SiS_StandTable[0x11].CRTC[16] = 0x82;
      SiS_StandTable[0x11].CRTC[17] = 0x84;
      SiS_StandTable[0x12].CRTC[4] = 0x55;
      SiS_StandTable[0x12].CRTC[5] = 0x81;
      SiS_StandTable[0x12].CRTC[16] = 0x82;
      SiS_StandTable[0x12].CRTC[17] = 0x84;
      SiS_StandTable[0x13].CRTC[5] = 0xb1;
      SiS_StandTable[0x17].CRTC[5] = 0xb1;
      SiS_StandTable[0x1a].CRTC[4] = 0x55;
      SiS_StandTable[0x1a].CRTC[5] = 0x81;
      SiS_StandTable[0x1a].CRTC[16] = 0xe9;
      SiS_StandTable[0x1a].CRTC[17] = 0x8b;
      SiS_StandTable[0x1b].CRTC[4] = 0x55;
      SiS_StandTable[0x1b].CRTC[5] = 0x81;
      SiS_StandTable[0x1b].CRTC[16] = 0xe9;
      SiS_StandTable[0x1b].CRTC[17] = 0x8b;
      SiS_StandTable[0x1c].CRTC[4] = 0x55;
      SiS_StandTable[0x1c].CRTC[5] = 0x81;
   }

   SiS_Pr->SiS_NTSCPhase    = SiS_NTSCPhase;
   SiS_Pr->SiS_PALPhase     = SiS_PALPhase;
   SiS_Pr->SiS_NTSCPhase2   = SiS_NTSCPhase2;
   SiS_Pr->SiS_PALPhase2    = SiS_PALPhase2;
   SiS_Pr->SiS_PALMPhase    = SiS_PALMPhase;
   SiS_Pr->SiS_PALNPhase    = SiS_PALNPhase;
   SiS_Pr->SiS_PALMPhase2   = SiS_PALMPhase2;
   SiS_Pr->SiS_PALNPhase2   = SiS_PALNPhase2;
   SiS_Pr->SiS_SpecialPhase = SiS_SpecialPhase;

   SiS_Pr->SiS_NTSCTiming     = SiS_NTSCTiming;
   SiS_Pr->SiS_PALTiming      = SiS_PALTiming;
   SiS_Pr->SiS_HiTVSt1Timing  = SiS_HiTVSt1Timing;
   SiS_Pr->SiS_HiTVSt2Timing  = SiS_HiTVSt2Timing;
   SiS_Pr->SiS_HiTVTextTiming = SiS_HiTVTextTiming;
   SiS_Pr->SiS_HiTVExtTiming  = SiS_HiTVExtTiming;
   SiS_Pr->SiS_HiTVGroup3Data = SiS_HiTVGroup3Data;
   SiS_Pr->SiS_HiTVGroup3Simu = SiS_HiTVGroup3Simu;
   SiS_Pr->SiS_HiTVGroup3Text = SiS_HiTVGroup3Text;

   SiS_Pr->SiS_StPALData   = SiS_StPALData;
   SiS_Pr->SiS_ExtPALData  = SiS_ExtPALData;
   SiS_Pr->SiS_StNTSCData  = SiS_StNTSCData;
   SiS_Pr->SiS_ExtNTSCData = SiS_ExtNTSCData;
/* SiS_Pr->SiS_St1HiTVData = SiS_St1HiTVData;  */
   SiS_Pr->SiS_St2HiTVData = SiS_St2HiTVData;
   SiS_Pr->SiS_ExtHiTVData = SiS_ExtHiTVData;

   SiS_Pr->pSiS_OutputSelect = &SiS_OutputSelect;
   SiS_Pr->pSiS_SoftSetting  = &SiS_SoftSetting;

   SiS_Pr->SiS_LCD1280x960Data      = SiS_LCD1280x960Data;
   SiS_Pr->SiS_ExtLCD1400x1050Data  = SiS_ExtLCD1400x1050Data;
   SiS_Pr->SiS_ExtLCD1600x1200Data  = SiS_ExtLCD1600x1200Data;
   SiS_Pr->SiS_StLCD1400x1050Data   = SiS_StLCD1400x1050Data;
   SiS_Pr->SiS_StLCD1600x1200Data   = SiS_StLCD1600x1200Data;
   SiS_Pr->SiS_NoScaleData1400x1050 = SiS_NoScaleData1400x1050;
   SiS_Pr->SiS_NoScaleData1600x1200 = SiS_NoScaleData1600x1200;
   SiS_Pr->SiS_ExtLCD1280x768Data   = SiS_ExtLCD1280x768Data;
   SiS_Pr->SiS_StLCD1280x768Data    = SiS_StLCD1280x768Data;
   SiS_Pr->SiS_NoScaleData1280x768  = SiS_NoScaleData1280x768;
   SiS_Pr->SiS_NoScaleData          = SiS_NoScaleData;

   SiS_Pr->SiS_LVDS320x480Data_1   = SiS_LVDS320x480Data_1;
   SiS_Pr->SiS_LVDS800x600Data_1   = SiS_LVDS800x600Data_1;
   SiS_Pr->SiS_LVDS800x600Data_2   = SiS_LVDS800x600Data_2;
   SiS_Pr->SiS_LVDS1024x768Data_1  = SiS_LVDS1024x768Data_1;
   SiS_Pr->SiS_LVDS1024x768Data_2  = SiS_LVDS1024x768Data_2;
   SiS_Pr->SiS_LVDS1280x1024Data_1 = SiS_LVDS1280x1024Data_1;
   SiS_Pr->SiS_LVDS1280x1024Data_2 = SiS_LVDS1280x1024Data_2;
   SiS_Pr->SiS_LVDS1400x1050Data_1 = SiS_LVDS1400x1050Data_1;
   SiS_Pr->SiS_LVDS1400x1050Data_2 = SiS_LVDS1400x1050Data_2;
   SiS_Pr->SiS_LVDS1600x1200Data_1 = SiS_LVDS1600x1200Data_1;
   SiS_Pr->SiS_LVDS1600x1200Data_2 = SiS_LVDS1600x1200Data_2;
   SiS_Pr->SiS_LVDS1280x768Data_1  = SiS_LVDS1280x768Data_1;
   SiS_Pr->SiS_LVDS1280x768Data_2  = SiS_LVDS1280x768Data_2;
   SiS_Pr->SiS_LVDS1024x600Data_1  = SiS_LVDS1024x600Data_1;
   SiS_Pr->SiS_LVDS1024x600Data_2  = SiS_LVDS1024x600Data_2;
   SiS_Pr->SiS_LVDS1152x768Data_1  = SiS_LVDS1152x768Data_1;
   SiS_Pr->SiS_LVDS1152x768Data_2  = SiS_LVDS1152x768Data_2;
   SiS_Pr->SiS_LVDSXXXxXXXData_1   = SiS_LVDSXXXxXXXData_1;
   SiS_Pr->SiS_LVDS1280x960Data_1  = SiS_LVDS1280x960Data_1;
   SiS_Pr->SiS_LVDS1280x960Data_2  = SiS_LVDS1280x960Data_2;
   SiS_Pr->SiS_LVDS640x480Data_1   = SiS_LVDS640x480Data_1;
   SiS_Pr->SiS_LVDS1280x960Data_1  = SiS_LVDS1280x1024Data_1;
   SiS_Pr->SiS_LVDS1280x960Data_2  = SiS_LVDS1280x1024Data_2;
   SiS_Pr->SiS_LVDS640x480Data_1   = SiS_LVDS640x480Data_1;
   SiS_Pr->SiS_LVDS640x480Data_2   = SiS_LVDS640x480Data_2;

   SiS_Pr->SiS_LVDSBARCO1366Data_1 = SiS_LVDSBARCO1366Data_1;
   SiS_Pr->SiS_LVDSBARCO1366Data_2 = SiS_LVDSBARCO1366Data_2;
   SiS_Pr->SiS_LVDSBARCO1024Data_1 = SiS_LVDSBARCO1024Data_1;
   SiS_Pr->SiS_LVDSBARCO1024Data_2 = SiS_LVDSBARCO1024Data_2;
   SiS_Pr->SiS_LVDS848x480Data_1   = SiS_LVDS848x480Data_1;
   SiS_Pr->SiS_LVDS848x480Data_2   = SiS_LVDS848x480Data_2;

   SiS_Pr->SiS_LCDA1400x1050Data_1 = SiS_LCDA1400x1050Data_1;
   SiS_Pr->SiS_LCDA1400x1050Data_2 = SiS_LCDA1400x1050Data_2;
   SiS_Pr->SiS_LCDA1600x1200Data_1 = SiS_LCDA1600x1200Data_1;
   SiS_Pr->SiS_LCDA1600x1200Data_2 = SiS_LCDA1600x1200Data_2;
   SiS_Pr->SiS_CHTVUNTSCData = SiS_CHTVUNTSCData;
   SiS_Pr->SiS_CHTVONTSCData = SiS_CHTVONTSCData;

   SiS_Pr->LVDS1024x768Des_1  = SiS_PanelType1076_1;
   SiS_Pr->LVDS1280x1024Des_1 = SiS_PanelType1210_1;
   SiS_Pr->LVDS1400x1050Des_1 = SiS_PanelType1296_1;
   SiS_Pr->LVDS1600x1200Des_1 = SiS_PanelType1600_1;
   SiS_Pr->LVDS1024x768Des_2  = SiS_PanelType1076_2;
   SiS_Pr->LVDS1280x1024Des_2 = SiS_PanelType1210_2;
   SiS_Pr->LVDS1400x1050Des_2 = SiS_PanelType1296_2;
   SiS_Pr->LVDS1600x1200Des_2 = SiS_PanelType1600_2;

   SiS_Pr->SiS_PanelTypeNS_1 = SiS_PanelTypeNS_1;
   SiS_Pr->SiS_PanelTypeNS_2 = SiS_PanelTypeNS_2;

   SiS_Pr->SiS_CHTVUNTSCDesData = SiS_CHTVUNTSCDesData;
   SiS_Pr->SiS_CHTVONTSCDesData = SiS_CHTVONTSCDesData;
   SiS_Pr->SiS_CHTVUPALDesData  = SiS_CHTVUPALDesData;
   SiS_Pr->SiS_CHTVOPALDesData  = SiS_CHTVOPALDesData;

   SiS_Pr->SiS_LVDSCRT11280x768_1    = SiS_LVDSCRT11280x768_1;
   SiS_Pr->SiS_LVDSCRT11024x600_1    = SiS_LVDSCRT11024x600_1;
   SiS_Pr->SiS_LVDSCRT11152x768_1    = SiS_LVDSCRT11152x768_1;
   SiS_Pr->SiS_LVDSCRT11280x768_1_H  = SiS_LVDSCRT11280x768_1_H;
   SiS_Pr->SiS_LVDSCRT11024x600_1_H  = SiS_LVDSCRT11024x600_1_H;
   SiS_Pr->SiS_LVDSCRT11152x768_1_H  = SiS_LVDSCRT11152x768_1_H;
   SiS_Pr->SiS_LVDSCRT11280x768_2    = SiS_LVDSCRT11280x768_2;
   SiS_Pr->SiS_LVDSCRT11024x600_2    = SiS_LVDSCRT11024x600_2;
   SiS_Pr->SiS_LVDSCRT11152x768_2    = SiS_LVDSCRT11152x768_2;
   SiS_Pr->SiS_LVDSCRT11280x768_2_H  = SiS_LVDSCRT11280x768_2_H;
   SiS_Pr->SiS_LVDSCRT11024x600_2_H  = SiS_LVDSCRT11024x600_2_H;
   SiS_Pr->SiS_LVDSCRT11152x768_2_H  = SiS_LVDSCRT11152x768_2_H;
   SiS_Pr->SiS_LVDSCRT1320x480_1     = SiS_LVDSCRT1320x480_1;
   SiS_Pr->SiS_LVDSCRT1XXXxXXX_1     = SiS_LVDSCRT1XXXxXXX_1;
   SiS_Pr->SiS_LVDSCRT1XXXxXXX_1_H   = SiS_LVDSCRT1XXXxXXX_1_H;
   SiS_Pr->SiS_LVDSCRT1640x480_1     = SiS_LVDSCRT1640x480_1;
   SiS_Pr->SiS_LVDSCRT1640x480_1_H   = SiS_LVDSCRT1640x480_1_H;
   SiS_Pr->SiS_LVDSCRT1640x480_2     = SiS_LVDSCRT1640x480_2;
   SiS_Pr->SiS_LVDSCRT1640x480_2_H   = SiS_LVDSCRT1640x480_2_H;
   SiS_Pr->SiS_LVDSCRT1640x480_3     = SiS_LVDSCRT1640x480_3;
   SiS_Pr->SiS_LVDSCRT1640x480_3_H   = SiS_LVDSCRT1640x480_3_H;
}

#ifdef SIS300
static void
InitTo300Pointer(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
   InitCommonPointer(SiS_Pr, HwDeviceExtension);

   SiS_Pr->SiS_SModeIDTable  = (SiS_StStruct *)SiS300_SModeIDTable;
   SiS_Pr->SiS_VBModeIDTable = (SiS_VBModeStruct *)SiS300_VBModeIDTable;
   SiS_Pr->SiS_EModeIDTable  = (SiS_ExtStruct *)SiS300_EModeIDTable;
   SiS_Pr->SiS_RefIndex      = (SiS_Ext2Struct *)SiS300_RefIndex;
   SiS_Pr->SiS_CRT1Table     = (SiS_CRT1TableStruct *)SiS300_CRT1Table;
   if(HwDeviceExtension->jChipType == SIS_300) {
      SiS_Pr->SiS_MCLKData_0    = (SiS_MCLKDataStruct *)SiS300_MCLKData_300; /* 300 */
   } else {
      SiS_Pr->SiS_MCLKData_0    = (SiS_MCLKDataStruct *)SiS300_MCLKData_630; /* 630, 730 */
   }
#ifdef LINUXBIOS
   SiS_Pr->SiS_ECLKData      = (SiS_ECLKDataStruct *)SiS300_ECLKData;
#endif
   SiS_Pr->SiS_VCLKData      = (SiS_VCLKDataStruct *)SiS300_VCLKData;
   SiS_Pr->SiS_VBVCLKData    = (SiS_VBVCLKDataStruct *)SiS300_VCLKData;
   SiS_Pr->SiS_ScreenOffset  = SiS300_ScreenOffset;

   SiS_Pr->SiS_SR15  = SiS300_SR15;

#ifndef LINUX_XF86
   SiS_Pr->pSiS_SR07 = &SiS300_SR07;
   SiS_Pr->SiS_CR40  = SiS300_CR40;
   SiS_Pr->SiS_CR49  = SiS300_CR49;
   SiS_Pr->pSiS_SR1F = &SiS300_SR1F;
   SiS_Pr->pSiS_SR21 = &SiS300_SR21;
   SiS_Pr->pSiS_SR22 = &SiS300_SR22;
   SiS_Pr->pSiS_SR23 = &SiS300_SR23;
   SiS_Pr->pSiS_SR24 = &SiS300_SR24;
   SiS_Pr->SiS_SR25  = SiS300_SR25;
   SiS_Pr->pSiS_SR31 = &SiS300_SR31;
   SiS_Pr->pSiS_SR32 = &SiS300_SR32;
   SiS_Pr->pSiS_SR33 = &SiS300_SR33;
   SiS_Pr->pSiS_CRT2Data_1_2  = &SiS300_CRT2Data_1_2;
   SiS_Pr->pSiS_CRT2Data_4_D  = &SiS300_CRT2Data_4_D;
   SiS_Pr->pSiS_CRT2Data_4_E  = &SiS300_CRT2Data_4_E;
   SiS_Pr->pSiS_CRT2Data_4_10 = &SiS300_CRT2Data_4_10;
   SiS_Pr->pSiS_RGBSenseData    = &SiS300_RGBSenseData;
   SiS_Pr->pSiS_VideoSenseData  = &SiS300_VideoSenseData;
   SiS_Pr->pSiS_YCSenseData     = &SiS300_YCSenseData;
   SiS_Pr->pSiS_RGBSenseData2   = &SiS300_RGBSenseData2;
   SiS_Pr->pSiS_VideoSenseData2 = &SiS300_VideoSenseData2;
   SiS_Pr->pSiS_YCSenseData2    = &SiS300_YCSenseData2;
#endif

   SiS_Pr->SiS_StLCD1024x768Data    = (SiS_LCDDataStruct *)SiS300_StLCD1024x768Data;
   SiS_Pr->SiS_ExtLCD1024x768Data   = (SiS_LCDDataStruct *)SiS300_ExtLCD1024x768Data;
   SiS_Pr->SiS_St2LCD1024x768Data   = (SiS_LCDDataStruct *)SiS300_St2LCD1024x768Data;
   SiS_Pr->SiS_StLCD1280x1024Data   = (SiS_LCDDataStruct *)SiS300_StLCD1280x1024Data;
   SiS_Pr->SiS_ExtLCD1280x1024Data  = (SiS_LCDDataStruct *)SiS300_ExtLCD1280x1024Data;
   SiS_Pr->SiS_St2LCD1280x1024Data  = (SiS_LCDDataStruct *)SiS300_St2LCD1280x1024Data;
   SiS_Pr->SiS_NoScaleData1024x768  = (SiS_LCDDataStruct *)SiS300_NoScaleData1024x768;
   SiS_Pr->SiS_NoScaleData1280x1024 = (SiS_LCDDataStruct *)SiS300_NoScaleData1280x1024;

   SiS_Pr->SiS_PanelDelayTbl     = (SiS_PanelDelayTblStruct *)SiS300_PanelDelayTbl;
   SiS_Pr->SiS_PanelDelayTblLVDS = (SiS_PanelDelayTblStruct *)SiS300_PanelDelayTblLVDS;

   SiS_Pr->SiS_CHTVUPALData  = (SiS_LVDSDataStruct *)SiS300_CHTVUPALData;
   SiS_Pr->SiS_CHTVOPALData  = (SiS_LVDSDataStruct *)SiS300_CHTVOPALData;
   SiS_Pr->SiS_CHTVUPALMData = SiS_CHTVUNTSCData; 			   /* not supported on 300 series */
   SiS_Pr->SiS_CHTVOPALMData = SiS_CHTVONTSCData; 			   /* not supported on 300 series */
   SiS_Pr->SiS_CHTVUPALNData = (SiS_LVDSDataStruct *)SiS300_CHTVUPALData;  /* not supported on 300 series */
   SiS_Pr->SiS_CHTVOPALNData = (SiS_LVDSDataStruct *)SiS300_CHTVOPALData;  /* not supported on 300 series */
   SiS_Pr->SiS_CHTVSOPALData = (SiS_LVDSDataStruct *)SiS300_CHTVSOPALData;

   SiS_Pr->SiS_PanelType00_1 = (SiS_LVDSDesStruct *)SiS300_PanelType00_1;
   SiS_Pr->SiS_PanelType01_1 = (SiS_LVDSDesStruct *)SiS300_PanelType01_1;
   SiS_Pr->SiS_PanelType02_1 = (SiS_LVDSDesStruct *)SiS300_PanelType02_1;
   SiS_Pr->SiS_PanelType03_1 = (SiS_LVDSDesStruct *)SiS300_PanelType03_1;
   SiS_Pr->SiS_PanelType04_1 = (SiS_LVDSDesStruct *)SiS300_PanelType04_1;
   SiS_Pr->SiS_PanelType05_1 = (SiS_LVDSDesStruct *)SiS300_PanelType05_1;
   SiS_Pr->SiS_PanelType06_1 = (SiS_LVDSDesStruct *)SiS300_PanelType06_1;
   SiS_Pr->SiS_PanelType07_1 = (SiS_LVDSDesStruct *)SiS300_PanelType07_1;
   SiS_Pr->SiS_PanelType08_1 = (SiS_LVDSDesStruct *)SiS300_PanelType08_1;
   SiS_Pr->SiS_PanelType09_1 = (SiS_LVDSDesStruct *)SiS300_PanelType09_1;
   SiS_Pr->SiS_PanelType0a_1 = (SiS_LVDSDesStruct *)SiS300_PanelType0a_1;
   SiS_Pr->SiS_PanelType0b_1 = (SiS_LVDSDesStruct *)SiS300_PanelType0b_1;
   SiS_Pr->SiS_PanelType0c_1 = (SiS_LVDSDesStruct *)SiS300_PanelType0c_1;
   SiS_Pr->SiS_PanelType0d_1 = (SiS_LVDSDesStruct *)SiS300_PanelType0d_1;
   SiS_Pr->SiS_PanelType0e_1 = (SiS_LVDSDesStruct *)SiS300_PanelType0e_1;
   SiS_Pr->SiS_PanelType0f_1 = (SiS_LVDSDesStruct *)SiS300_PanelType0f_1;
   SiS_Pr->SiS_PanelType00_2 = (SiS_LVDSDesStruct *)SiS300_PanelType00_2;
   SiS_Pr->SiS_PanelType01_2 = (SiS_LVDSDesStruct *)SiS300_PanelType01_2;
   SiS_Pr->SiS_PanelType02_2 = (SiS_LVDSDesStruct *)SiS300_PanelType02_2;
   SiS_Pr->SiS_PanelType03_2 = (SiS_LVDSDesStruct *)SiS300_PanelType03_2;
   SiS_Pr->SiS_PanelType04_2 = (SiS_LVDSDesStruct *)SiS300_PanelType04_2;
   SiS_Pr->SiS_PanelType05_2 = (SiS_LVDSDesStruct *)SiS300_PanelType05_2;
   SiS_Pr->SiS_PanelType06_2 = (SiS_LVDSDesStruct *)SiS300_PanelType06_2;
   SiS_Pr->SiS_PanelType07_2 = (SiS_LVDSDesStruct *)SiS300_PanelType07_2;
   SiS_Pr->SiS_PanelType08_2 = (SiS_LVDSDesStruct *)SiS300_PanelType08_2;
   SiS_Pr->SiS_PanelType09_2 = (SiS_LVDSDesStruct *)SiS300_PanelType09_2;
   SiS_Pr->SiS_PanelType0a_2 = (SiS_LVDSDesStruct *)SiS300_PanelType0a_2;
   SiS_Pr->SiS_PanelType0b_2 = (SiS_LVDSDesStruct *)SiS300_PanelType0b_2;
   SiS_Pr->SiS_PanelType0c_2 = (SiS_LVDSDesStruct *)SiS300_PanelType0c_2;
   SiS_Pr->SiS_PanelType0d_2 = (SiS_LVDSDesStruct *)SiS300_PanelType0d_2;
   SiS_Pr->SiS_PanelType0e_2 = (SiS_LVDSDesStruct *)SiS300_PanelType0e_2;
   SiS_Pr->SiS_PanelType0f_2 = (SiS_LVDSDesStruct *)SiS300_PanelType0f_2;

   if(SiS_Pr->SiS_CustomT == CUT_BARCO1366) {
      SiS_Pr->SiS_PanelType04_1 = (SiS_LVDSDesStruct *)SiS300_PanelType04_1a;
      SiS_Pr->SiS_PanelType04_2 = (SiS_LVDSDesStruct *)SiS300_PanelType04_2a;
   }
   if(SiS_Pr->SiS_CustomT == CUT_BARCO1024) {
      SiS_Pr->SiS_PanelType04_1 = (SiS_LVDSDesStruct *)SiS300_PanelType04_1b;
      SiS_Pr->SiS_PanelType04_2 = (SiS_LVDSDesStruct *)SiS300_PanelType04_2b;
   }

   SiS_Pr->SiS_LVDSCRT1800x600_1     = (SiS_LVDSCRT1DataStruct *)SiS300_LVDSCRT1800x600_1;
   SiS_Pr->SiS_LVDSCRT11024x768_1    = (SiS_LVDSCRT1DataStruct *)SiS300_LVDSCRT11024x768_1;
   SiS_Pr->SiS_LVDSCRT11280x1024_1   = (SiS_LVDSCRT1DataStruct *)SiS300_LVDSCRT11280x1024_1;
   SiS_Pr->SiS_LVDSCRT1800x600_1_H   = (SiS_LVDSCRT1DataStruct *)SiS300_LVDSCRT1800x600_1_H;
   SiS_Pr->SiS_LVDSCRT11024x768_1_H  = (SiS_LVDSCRT1DataStruct *)SiS300_LVDSCRT11024x768_1_H;
   SiS_Pr->SiS_LVDSCRT11280x1024_1_H = (SiS_LVDSCRT1DataStruct *)SiS300_LVDSCRT11280x1024_1_H;
   SiS_Pr->SiS_LVDSCRT1800x600_2     = (SiS_LVDSCRT1DataStruct *)SiS300_LVDSCRT1800x600_2;
   SiS_Pr->SiS_LVDSCRT11024x768_2    = (SiS_LVDSCRT1DataStruct *)SiS300_LVDSCRT11024x768_2;
   SiS_Pr->SiS_LVDSCRT11280x1024_2   = (SiS_LVDSCRT1DataStruct *)SiS300_LVDSCRT11280x1024_2;
   SiS_Pr->SiS_LVDSCRT1800x600_2_H   = (SiS_LVDSCRT1DataStruct *)SiS300_LVDSCRT1800x600_2_H;
   SiS_Pr->SiS_LVDSCRT11024x768_2_H  = (SiS_LVDSCRT1DataStruct *)SiS300_LVDSCRT11024x768_2_H;
   SiS_Pr->SiS_LVDSCRT11280x1024_2_H = (SiS_LVDSCRT1DataStruct *)SiS300_LVDSCRT11280x1024_2_H;
   SiS_Pr->SiS_CHTVCRT1UNTSC = (SiS_LVDSCRT1DataStruct *)SiS300_CHTVCRT1UNTSC;
   SiS_Pr->SiS_CHTVCRT1ONTSC = (SiS_LVDSCRT1DataStruct *)SiS300_CHTVCRT1ONTSC;
   SiS_Pr->SiS_CHTVCRT1UPAL  = (SiS_LVDSCRT1DataStruct *)SiS300_CHTVCRT1UPAL;
   SiS_Pr->SiS_CHTVCRT1OPAL  = (SiS_LVDSCRT1DataStruct *)SiS300_CHTVCRT1OPAL;
   SiS_Pr->SiS_CHTVCRT1SOPAL = (SiS_LVDSCRT1DataStruct *)SiS300_CHTVCRT1SOPAL;
   SiS_Pr->SiS_CHTVReg_UNTSC = (SiS_CHTVRegDataStruct *)SiS300_CHTVReg_UNTSC;
   SiS_Pr->SiS_CHTVReg_ONTSC = (SiS_CHTVRegDataStruct *)SiS300_CHTVReg_ONTSC;
   SiS_Pr->SiS_CHTVReg_UPAL  = (SiS_CHTVRegDataStruct *)SiS300_CHTVReg_UPAL;
   SiS_Pr->SiS_CHTVReg_OPAL  = (SiS_CHTVRegDataStruct *)SiS300_CHTVReg_OPAL;
   SiS_Pr->SiS_CHTVReg_UPALM = (SiS_CHTVRegDataStruct *)SiS300_CHTVReg_UNTSC;  /* not supported on 300 series */
   SiS_Pr->SiS_CHTVReg_OPALM = (SiS_CHTVRegDataStruct *)SiS300_CHTVReg_ONTSC;  /* not supported on 300 series */
   SiS_Pr->SiS_CHTVReg_UPALN = (SiS_CHTVRegDataStruct *)SiS300_CHTVReg_UPAL;   /* not supported on 300 series */
   SiS_Pr->SiS_CHTVReg_OPALN = (SiS_CHTVRegDataStruct *)SiS300_CHTVReg_OPAL;   /* not supported on 300 series */
   SiS_Pr->SiS_CHTVReg_SOPAL = (SiS_CHTVRegDataStruct *)SiS300_CHTVReg_SOPAL;
   SiS_Pr->SiS_CHTVVCLKUNTSC = SiS300_CHTVVCLKUNTSC;
   SiS_Pr->SiS_CHTVVCLKONTSC = SiS300_CHTVVCLKONTSC;
   SiS_Pr->SiS_CHTVVCLKUPAL  = SiS300_CHTVVCLKUPAL;
   SiS_Pr->SiS_CHTVVCLKOPAL  = SiS300_CHTVVCLKOPAL;
   SiS_Pr->SiS_CHTVVCLKUPALM = SiS300_CHTVVCLKUNTSC;  /* not supported on 300 series */
   SiS_Pr->SiS_CHTVVCLKOPALM = SiS300_CHTVVCLKONTSC;  /* not supported on 300 series */
   SiS_Pr->SiS_CHTVVCLKUPALN = SiS300_CHTVVCLKUPAL;   /* not supported on 300 series */
   SiS_Pr->SiS_CHTVVCLKOPALN = SiS300_CHTVVCLKOPAL;   /* not supported on 300 series */
   SiS_Pr->SiS_CHTVVCLKSOPAL = SiS300_CHTVVCLKSOPAL;

   SiS_Pr->SiS_CRT2Part2_1024x768_1  = (SiS_Part2PortTblStruct *)SiS300_CRT2Part2_1024x768_1;
   SiS_Pr->SiS_CRT2Part2_1280x1024_1 = (SiS_Part2PortTblStruct *)SiS300_CRT2Part2_1280x1024_1;
   SiS_Pr->SiS_CRT2Part2_1400x1050_1 = (SiS_Part2PortTblStruct *)SiS300_CRT2Part2_1400x1050_1;
   SiS_Pr->SiS_CRT2Part2_1600x1200_1 = (SiS_Part2PortTblStruct *)SiS300_CRT2Part2_1600x1200_1;
   SiS_Pr->SiS_CRT2Part2_1024x768_2  = (SiS_Part2PortTblStruct *)SiS300_CRT2Part2_1024x768_2;
   SiS_Pr->SiS_CRT2Part2_1280x1024_2 = (SiS_Part2PortTblStruct *)SiS300_CRT2Part2_1280x1024_2;
   SiS_Pr->SiS_CRT2Part2_1400x1050_2 = (SiS_Part2PortTblStruct *)SiS300_CRT2Part2_1400x1050_2;
   SiS_Pr->SiS_CRT2Part2_1600x1200_2 = (SiS_Part2PortTblStruct *)SiS300_CRT2Part2_1600x1200_2;
   SiS_Pr->SiS_CRT2Part2_1024x768_3  = (SiS_Part2PortTblStruct *)SiS300_CRT2Part2_1024x768_3;
   SiS_Pr->SiS_CRT2Part2_1280x1024_3 = (SiS_Part2PortTblStruct *)SiS300_CRT2Part2_1280x1024_3;
   SiS_Pr->SiS_CRT2Part2_1400x1050_3 = (SiS_Part2PortTblStruct *)SiS300_CRT2Part2_1400x1050_3;
   SiS_Pr->SiS_CRT2Part2_1600x1200_3 = (SiS_Part2PortTblStruct *)SiS300_CRT2Part2_1600x1200_3;

   /* TW: LCDResInfo will on 300 series be translated to 315 series definitions */
   SiS_Pr->SiS_Panel320x480   = Panel_320x480;
   SiS_Pr->SiS_Panel640x480   = Panel_640x480;
   SiS_Pr->SiS_Panel800x600   = Panel_800x600;
   SiS_Pr->SiS_Panel1024x768  = Panel_1024x768;
   SiS_Pr->SiS_Panel1280x1024 = Panel_1280x1024;
   SiS_Pr->SiS_Panel1280x960  = Panel_1280x960;
   SiS_Pr->SiS_Panel1024x600  = Panel_1024x600;
   SiS_Pr->SiS_Panel1152x768  = Panel_1152x768;
   SiS_Pr->SiS_Panel1280x768  = Panel_1280x768;
   SiS_Pr->SiS_Panel1600x1200 = 255;  		/* TW: Something illegal */
   SiS_Pr->SiS_Panel1400x1050 = 255;  		/* TW: Something illegal */
   SiS_Pr->SiS_Panel640x480_2 = 255;  		/* TW: Something illegal */
   SiS_Pr->SiS_Panel640x480_3 = 255;  		/* TW: Something illegal */
   SiS_Pr->SiS_Panel1152x864  = 255;   		/* TW: Something illegal */
   SiS_Pr->SiS_PanelMax       = Panel_320x480;     /* TW: highest value */
   SiS_Pr->SiS_PanelMinLVDS   = Panel_800x600;     /* TW: Lowest value LVDS */
   SiS_Pr->SiS_PanelMin301    = Panel_1024x768;    /* TW: lowest value 301 */
   SiS_Pr->SiS_PanelCustom    = Panel_Custom;
   SiS_Pr->SiS_PanelBarco1366 = Panel_Barco1366;
}
#endif

#ifdef SIS315H
static void
InitTo310Pointer(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
   InitCommonPointer(SiS_Pr, HwDeviceExtension);

   SiS_Pr->SiS_SModeIDTable  = (SiS_StStruct *)SiS310_SModeIDTable;
   SiS_Pr->SiS_EModeIDTable  = (SiS_ExtStruct *)SiS310_EModeIDTable;
   SiS_Pr->SiS_RefIndex      = (SiS_Ext2Struct *)SiS310_RefIndex;
   SiS_Pr->SiS_CRT1Table     = (SiS_CRT1TableStruct *)SiS310_CRT1Table;
   /* TW: MCLK is different */
#ifdef LINUXBIOS
   if(HwDeviceExtension->jChipType >= SIS_660) {
      SiS_Pr->SiS_MCLKData_0 = (SiS_MCLKDataStruct *)SiS310_MCLKData_0_660;  /* 660/760 */
   } else if(HwDeviceExtension->jChipType == SIS_330) {
#endif
      SiS_Pr->SiS_MCLKData_0 = (SiS_MCLKDataStruct *)SiS310_MCLKData_0_330;  /* 330 */
#ifdef LINUXBIOS
   } else if(HwDeviceExtension->jChipType > SIS_315PRO) {
      SiS_Pr->SiS_MCLKData_0 = (SiS_MCLKDataStruct *)SiS310_MCLKData_0_650;  /* 550, 650, 740 */
   } else {
      SiS_Pr->SiS_MCLKData_0 = (SiS_MCLKDataStruct *)SiS310_MCLKData_0_315;  /* 315 */
   }
#endif
   SiS_Pr->SiS_MCLKData_1    = (SiS_MCLKDataStruct *)SiS310_MCLKData_1;
#ifdef LINUXBIOS
   SiS_Pr->SiS_ECLKData      = (SiS_ECLKDataStruct *)SiS310_ECLKData;
#endif
   SiS_Pr->SiS_VCLKData      = (SiS_VCLKDataStruct *)SiS310_VCLKData;
   SiS_Pr->SiS_VBVCLKData    = (SiS_VBVCLKDataStruct *)SiS310_VBVCLKData;
   SiS_Pr->SiS_ScreenOffset  = SiS310_ScreenOffset;

   SiS_Pr->SiS_SR15  = SiS310_SR15;

#ifndef LINUX_XF86
   SiS_Pr->pSiS_SR07 = &SiS310_SR07;
   SiS_Pr->SiS_CR40  = SiS310_CR40;
   SiS_Pr->SiS_CR49  = SiS310_CR49;
   SiS_Pr->pSiS_SR1F = &SiS310_SR1F;
   SiS_Pr->pSiS_SR21 = &SiS310_SR21;
   SiS_Pr->pSiS_SR22 = &SiS310_SR22;
   SiS_Pr->pSiS_SR23 = &SiS310_SR23;
   SiS_Pr->pSiS_SR24 = &SiS310_SR24;
   SiS_Pr->SiS_SR25  = SiS310_SR25;
   SiS_Pr->pSiS_SR31 = &SiS310_SR31;
   SiS_Pr->pSiS_SR32 = &SiS310_SR32;
   SiS_Pr->pSiS_SR33 = &SiS310_SR33;
   SiS_Pr->pSiS_CRT2Data_1_2  = &SiS310_CRT2Data_1_2;
   SiS_Pr->pSiS_CRT2Data_4_D  = &SiS310_CRT2Data_4_D;
   SiS_Pr->pSiS_CRT2Data_4_E  = &SiS310_CRT2Data_4_E;
   SiS_Pr->pSiS_CRT2Data_4_10 = &SiS310_CRT2Data_4_10;
   SiS_Pr->pSiS_RGBSenseData    = &SiS310_RGBSenseData;
   SiS_Pr->pSiS_VideoSenseData  = &SiS310_VideoSenseData;
   SiS_Pr->pSiS_YCSenseData     = &SiS310_YCSenseData;
   SiS_Pr->pSiS_RGBSenseData2   = &SiS310_RGBSenseData2;
   SiS_Pr->pSiS_VideoSenseData2 = &SiS310_VideoSenseData2;
   SiS_Pr->pSiS_YCSenseData2    = &SiS310_YCSenseData2;
#endif

   SiS_Pr->SiS_StLCD1024x768Data    = (SiS_LCDDataStruct *)SiS310_StLCD1024x768Data;
   SiS_Pr->SiS_ExtLCD1024x768Data   = (SiS_LCDDataStruct *)SiS310_ExtLCD1024x768Data;
   SiS_Pr->SiS_St2LCD1024x768Data   = (SiS_LCDDataStruct *)SiS310_St2LCD1024x768Data;
   SiS_Pr->SiS_StLCD1280x1024Data   = (SiS_LCDDataStruct *)SiS310_StLCD1280x1024Data;
   SiS_Pr->SiS_ExtLCD1280x1024Data  = (SiS_LCDDataStruct *)SiS310_ExtLCD1280x1024Data;
   SiS_Pr->SiS_St2LCD1280x1024Data  = (SiS_LCDDataStruct *)SiS310_St2LCD1280x1024Data;
   SiS_Pr->SiS_NoScaleData1024x768  = (SiS_LCDDataStruct *)SiS310_NoScaleData1024x768;
   SiS_Pr->SiS_NoScaleData1280x1024 = (SiS_LCDDataStruct *)SiS310_NoScaleData1280x1024;

   SiS_Pr->SiS_PanelDelayTbl     = (SiS_PanelDelayTblStruct *)SiS310_PanelDelayTbl;
   SiS_Pr->SiS_PanelDelayTblLVDS = (SiS_PanelDelayTblStruct *)SiS310_PanelDelayTblLVDS;

   SiS_Pr->SiS_CHTVUPALData  = (SiS_LVDSDataStruct *)SiS310_CHTVUPALData;
   SiS_Pr->SiS_CHTVOPALData  = (SiS_LVDSDataStruct *)SiS310_CHTVOPALData;
   SiS_Pr->SiS_CHTVUPALMData = (SiS_LVDSDataStruct *)SiS310_CHTVUPALMData;
   SiS_Pr->SiS_CHTVOPALMData = (SiS_LVDSDataStruct *)SiS310_CHTVOPALMData;
   SiS_Pr->SiS_CHTVUPALNData = (SiS_LVDSDataStruct *)SiS310_CHTVUPALNData;
   SiS_Pr->SiS_CHTVOPALNData = (SiS_LVDSDataStruct *)SiS310_CHTVOPALNData;
   SiS_Pr->SiS_CHTVSOPALData = (SiS_LVDSDataStruct *)SiS310_CHTVSOPALData;

   SiS_Pr->SiS_PanelType00_1 = (SiS_LVDSDesStruct *)SiS310_PanelType00_1;
   SiS_Pr->SiS_PanelType01_1 = (SiS_LVDSDesStruct *)SiS310_PanelType01_1;
   SiS_Pr->SiS_PanelType02_1 = (SiS_LVDSDesStruct *)SiS310_PanelType02_1;
   SiS_Pr->SiS_PanelType03_1 = (SiS_LVDSDesStruct *)SiS310_PanelType03_1;
   SiS_Pr->SiS_PanelType04_1 = (SiS_LVDSDesStruct *)SiS310_PanelType04_1;
   SiS_Pr->SiS_PanelType05_1 = (SiS_LVDSDesStruct *)SiS310_PanelType05_1;
   SiS_Pr->SiS_PanelType06_1 = (SiS_LVDSDesStruct *)SiS310_PanelType06_1;
   SiS_Pr->SiS_PanelType07_1 = (SiS_LVDSDesStruct *)SiS310_PanelType07_1;
   SiS_Pr->SiS_PanelType08_1 = (SiS_LVDSDesStruct *)SiS310_PanelType08_1;
   SiS_Pr->SiS_PanelType09_1 = (SiS_LVDSDesStruct *)SiS310_PanelType09_1;
   SiS_Pr->SiS_PanelType0a_1 = (SiS_LVDSDesStruct *)SiS310_PanelType0a_1;
   SiS_Pr->SiS_PanelType0b_1 = (SiS_LVDSDesStruct *)SiS310_PanelType0b_1;
   SiS_Pr->SiS_PanelType0c_1 = (SiS_LVDSDesStruct *)SiS310_PanelType0c_1;
   SiS_Pr->SiS_PanelType0d_1 = (SiS_LVDSDesStruct *)SiS310_PanelType0d_1;
   SiS_Pr->SiS_PanelType0e_1 = (SiS_LVDSDesStruct *)SiS310_PanelType0e_1;
   SiS_Pr->SiS_PanelType0f_1 = (SiS_LVDSDesStruct *)SiS310_PanelType0f_1;
   SiS_Pr->SiS_PanelType00_2 = (SiS_LVDSDesStruct *)SiS310_PanelType00_2;
   SiS_Pr->SiS_PanelType01_2 = (SiS_LVDSDesStruct *)SiS310_PanelType01_2;
   SiS_Pr->SiS_PanelType02_2 = (SiS_LVDSDesStruct *)SiS310_PanelType02_2;
   SiS_Pr->SiS_PanelType03_2 = (SiS_LVDSDesStruct *)SiS310_PanelType03_2;
   SiS_Pr->SiS_PanelType04_2 = (SiS_LVDSDesStruct *)SiS310_PanelType04_2;
   SiS_Pr->SiS_PanelType05_2 = (SiS_LVDSDesStruct *)SiS310_PanelType05_2;
   SiS_Pr->SiS_PanelType06_2 = (SiS_LVDSDesStruct *)SiS310_PanelType06_2;
   SiS_Pr->SiS_PanelType07_2 = (SiS_LVDSDesStruct *)SiS310_PanelType07_2;
   SiS_Pr->SiS_PanelType08_2 = (SiS_LVDSDesStruct *)SiS310_PanelType08_2;
   SiS_Pr->SiS_PanelType09_2 = (SiS_LVDSDesStruct *)SiS310_PanelType09_2;
   SiS_Pr->SiS_PanelType0a_2 = (SiS_LVDSDesStruct *)SiS310_PanelType0a_2;
   SiS_Pr->SiS_PanelType0b_2 = (SiS_LVDSDesStruct *)SiS310_PanelType0b_2;
   SiS_Pr->SiS_PanelType0c_2 = (SiS_LVDSDesStruct *)SiS310_PanelType0c_2;
   SiS_Pr->SiS_PanelType0d_2 = (SiS_LVDSDesStruct *)SiS310_PanelType0d_2;
   SiS_Pr->SiS_PanelType0e_2 = (SiS_LVDSDesStruct *)SiS310_PanelType0e_2;
   SiS_Pr->SiS_PanelType0f_2 = (SiS_LVDSDesStruct *)SiS310_PanelType0f_2;

   SiS_Pr->SiS_CRT2Part2_1024x768_1  = (SiS_Part2PortTblStruct *)SiS310_CRT2Part2_1024x768_1;
   SiS_Pr->SiS_CRT2Part2_1280x1024_1 = (SiS_Part2PortTblStruct *)SiS310_CRT2Part2_1280x1024_1;
   SiS_Pr->SiS_CRT2Part2_1400x1050_1 = (SiS_Part2PortTblStruct *)SiS310_CRT2Part2_1400x1050_1;
   SiS_Pr->SiS_CRT2Part2_1600x1200_1 = (SiS_Part2PortTblStruct *)SiS310_CRT2Part2_1600x1200_1;
   SiS_Pr->SiS_CRT2Part2_1024x768_2  = (SiS_Part2PortTblStruct *)SiS310_CRT2Part2_1024x768_2;
   SiS_Pr->SiS_CRT2Part2_1280x1024_2 = (SiS_Part2PortTblStruct *)SiS310_CRT2Part2_1280x1024_2;
   SiS_Pr->SiS_CRT2Part2_1400x1050_2 = (SiS_Part2PortTblStruct *)SiS310_CRT2Part2_1400x1050_2;
   SiS_Pr->SiS_CRT2Part2_1600x1200_2 = (SiS_Part2PortTblStruct *)SiS310_CRT2Part2_1600x1200_2;
   SiS_Pr->SiS_CRT2Part2_1024x768_3  = (SiS_Part2PortTblStruct *)SiS310_CRT2Part2_1024x768_3;
   SiS_Pr->SiS_CRT2Part2_1280x1024_3 = (SiS_Part2PortTblStruct *)SiS310_CRT2Part2_1280x1024_3;
   SiS_Pr->SiS_CRT2Part2_1400x1050_3 = (SiS_Part2PortTblStruct *)SiS310_CRT2Part2_1400x1050_3;
   SiS_Pr->SiS_CRT2Part2_1600x1200_3 = (SiS_Part2PortTblStruct *)SiS310_CRT2Part2_1600x1200_3;

   SiS_Pr->SiS_LVDSCRT1800x600_1     = (SiS_LVDSCRT1DataStruct *)SiS310_LVDSCRT1800x600_1;
   SiS_Pr->SiS_LVDSCRT11024x768_1    = (SiS_LVDSCRT1DataStruct *)SiS310_LVDSCRT11024x768_1;
   SiS_Pr->SiS_LVDSCRT11280x1024_1   = (SiS_LVDSCRT1DataStruct *)SiS310_LVDSCRT11280x1024_1;
   SiS_Pr->SiS_LVDSCRT11400x1050_1   = (SiS_LVDSCRT1DataStruct *)SiS310_LVDSCRT11400x1050_1;
   SiS_Pr->SiS_LVDSCRT11600x1200_1   = (SiS_LVDSCRT1DataStruct *)SiS310_LVDSCRT11600x1200_1;
   SiS_Pr->SiS_LVDSCRT1800x600_1_H   = (SiS_LVDSCRT1DataStruct *)SiS310_LVDSCRT1800x600_1_H;
   SiS_Pr->SiS_LVDSCRT11024x768_1_H  = (SiS_LVDSCRT1DataStruct *)SiS310_LVDSCRT11024x768_1_H;
   SiS_Pr->SiS_LVDSCRT11280x1024_1_H = (SiS_LVDSCRT1DataStruct *)SiS310_LVDSCRT11280x1024_1_H;
   SiS_Pr->SiS_LVDSCRT11400x1050_1_H = (SiS_LVDSCRT1DataStruct *)SiS310_LVDSCRT11400x1050_1_H;
   SiS_Pr->SiS_LVDSCRT11600x1200_1_H = (SiS_LVDSCRT1DataStruct *)SiS310_LVDSCRT11600x1200_1_H;
   SiS_Pr->SiS_LVDSCRT1800x600_2     = (SiS_LVDSCRT1DataStruct *)SiS310_LVDSCRT1800x600_2;
   SiS_Pr->SiS_LVDSCRT11024x768_2    = (SiS_LVDSCRT1DataStruct *)SiS310_LVDSCRT11024x768_2;
   SiS_Pr->SiS_LVDSCRT11280x1024_2   = (SiS_LVDSCRT1DataStruct *)SiS310_LVDSCRT11280x1024_2;
   SiS_Pr->SiS_LVDSCRT11400x1050_2   = (SiS_LVDSCRT1DataStruct *)SiS310_LVDSCRT11400x1050_2;
   SiS_Pr->SiS_LVDSCRT11600x1200_2   = (SiS_LVDSCRT1DataStruct *)SiS310_LVDSCRT11600x1200_2;
   SiS_Pr->SiS_LVDSCRT1800x600_2_H   = (SiS_LVDSCRT1DataStruct *)SiS310_LVDSCRT1800x600_2_H;
   SiS_Pr->SiS_LVDSCRT11024x768_2_H  = (SiS_LVDSCRT1DataStruct *)SiS310_LVDSCRT11024x768_2_H;
   SiS_Pr->SiS_LVDSCRT11280x1024_2_H = (SiS_LVDSCRT1DataStruct *)SiS310_LVDSCRT11280x1024_2_H;
   SiS_Pr->SiS_LVDSCRT11400x1050_2_H = (SiS_LVDSCRT1DataStruct *)SiS310_LVDSCRT11400x1050_2_H;
   SiS_Pr->SiS_LVDSCRT11600x1200_2_H = (SiS_LVDSCRT1DataStruct *)SiS310_LVDSCRT11600x1200_2_H;
   SiS_Pr->SiS_CHTVCRT1UNTSC         = (SiS_LVDSCRT1DataStruct *)SiS310_CHTVCRT1UNTSC;
   SiS_Pr->SiS_CHTVCRT1ONTSC         = (SiS_LVDSCRT1DataStruct *)SiS310_CHTVCRT1ONTSC;
   SiS_Pr->SiS_CHTVCRT1UPAL          = (SiS_LVDSCRT1DataStruct *)SiS310_CHTVCRT1UPAL;
   SiS_Pr->SiS_CHTVCRT1OPAL          = (SiS_LVDSCRT1DataStruct *)SiS310_CHTVCRT1OPAL;
   SiS_Pr->SiS_CHTVCRT1SOPAL         = (SiS_LVDSCRT1DataStruct *)SiS310_CHTVCRT1SOPAL;

   SiS_Pr->SiS_CHTVReg_UNTSC = (SiS_CHTVRegDataStruct *)SiS310_CHTVReg_UNTSC;
   SiS_Pr->SiS_CHTVReg_ONTSC = (SiS_CHTVRegDataStruct *)SiS310_CHTVReg_ONTSC;
   SiS_Pr->SiS_CHTVReg_UPAL  = (SiS_CHTVRegDataStruct *)SiS310_CHTVReg_UPAL;
   SiS_Pr->SiS_CHTVReg_OPAL  = (SiS_CHTVRegDataStruct *)SiS310_CHTVReg_OPAL;
   SiS_Pr->SiS_CHTVReg_UPALM = (SiS_CHTVRegDataStruct *)SiS310_CHTVReg_UPALM;
   SiS_Pr->SiS_CHTVReg_OPALM = (SiS_CHTVRegDataStruct *)SiS310_CHTVReg_OPALM;
   SiS_Pr->SiS_CHTVReg_UPALN = (SiS_CHTVRegDataStruct *)SiS310_CHTVReg_UPALN;
   SiS_Pr->SiS_CHTVReg_OPALN = (SiS_CHTVRegDataStruct *)SiS310_CHTVReg_OPALN;
   SiS_Pr->SiS_CHTVReg_SOPAL = (SiS_CHTVRegDataStruct *)SiS310_CHTVReg_SOPAL;

   SiS_Pr->SiS_LCDACRT1800x600_1     = (SiS_LCDACRT1DataStruct *)SiS310_LCDACRT1800x600_1;
   SiS_Pr->SiS_LCDACRT11024x768_1    = (SiS_LCDACRT1DataStruct *)SiS310_LCDACRT11024x768_1;
   SiS_Pr->SiS_LCDACRT11280x1024_1   = (SiS_LCDACRT1DataStruct *)SiS310_LCDACRT11280x1024_1;
   SiS_Pr->SiS_LCDACRT11400x1050_1   = (SiS_LCDACRT1DataStruct *)SiS310_LCDACRT11400x1050_1;
   SiS_Pr->SiS_LCDACRT11600x1200_1   = (SiS_LCDACRT1DataStruct *)SiS310_LCDACRT11600x1200_1;
   SiS_Pr->SiS_LCDACRT1800x600_1_H   = (SiS_LCDACRT1DataStruct *)SiS310_LCDACRT1800x600_1_H;
   SiS_Pr->SiS_LCDACRT11024x768_1_H  = (SiS_LCDACRT1DataStruct *)SiS310_LCDACRT11024x768_1_H;
   SiS_Pr->SiS_LCDACRT11280x1024_1_H = (SiS_LCDACRT1DataStruct *)SiS310_LCDACRT11280x1024_1_H;
   SiS_Pr->SiS_LCDACRT11400x1050_1_H = (SiS_LCDACRT1DataStruct *)SiS310_LCDACRT11400x1050_1_H;
   SiS_Pr->SiS_LCDACRT11600x1200_1_H = (SiS_LCDACRT1DataStruct *)SiS310_LCDACRT11600x1200_1_H;
   SiS_Pr->SiS_LCDACRT1800x600_2     = (SiS_LCDACRT1DataStruct *)SiS310_LCDACRT1800x600_2;
   SiS_Pr->SiS_LCDACRT11024x768_2    = (SiS_LCDACRT1DataStruct *)SiS310_LCDACRT11024x768_2;
   SiS_Pr->SiS_LCDACRT11280x1024_2   = (SiS_LCDACRT1DataStruct *)SiS310_LCDACRT11280x1024_2;
   SiS_Pr->SiS_LCDACRT11400x1050_2   = (SiS_LCDACRT1DataStruct *)SiS310_LCDACRT11400x1050_2;
   SiS_Pr->SiS_LCDACRT11600x1200_2   = (SiS_LCDACRT1DataStruct *)SiS310_LCDACRT11600x1200_2;
   SiS_Pr->SiS_LCDACRT1800x600_2_H   = (SiS_LCDACRT1DataStruct *)SiS310_LCDACRT1800x600_2_H;
   SiS_Pr->SiS_LCDACRT11024x768_2_H  = (SiS_LCDACRT1DataStruct *)SiS310_LCDACRT11024x768_2_H;
   SiS_Pr->SiS_LCDACRT11280x1024_2_H = (SiS_LCDACRT1DataStruct *)SiS310_LCDACRT11280x1024_2_H;
   SiS_Pr->SiS_LCDACRT11400x1050_2_H = (SiS_LCDACRT1DataStruct *)SiS310_LCDACRT11400x1050_2_H;
   SiS_Pr->SiS_LCDACRT11600x1200_2_H = (SiS_LCDACRT1DataStruct *)SiS310_LCDACRT11600x1200_2_H;

   SiS_Pr->SiS_CHTVVCLKUNTSC = SiS310_CHTVVCLKUNTSC;
   SiS_Pr->SiS_CHTVVCLKONTSC = SiS310_CHTVVCLKONTSC;
   SiS_Pr->SiS_CHTVVCLKUPAL  = SiS310_CHTVVCLKUPAL;
   SiS_Pr->SiS_CHTVVCLKOPAL  = SiS310_CHTVVCLKOPAL;
   SiS_Pr->SiS_CHTVVCLKUPALM = SiS310_CHTVVCLKUPALM;
   SiS_Pr->SiS_CHTVVCLKOPALM = SiS310_CHTVVCLKOPALM;
   SiS_Pr->SiS_CHTVVCLKUPALN = SiS310_CHTVVCLKUPALN;
   SiS_Pr->SiS_CHTVVCLKOPALN = SiS310_CHTVVCLKOPALN;   
   SiS_Pr->SiS_CHTVVCLKSOPAL = SiS310_CHTVVCLKSOPAL;

   SiS_Pr->SiS_Panel320x480   = Panel_320x480;
   SiS_Pr->SiS_Panel640x480   = Panel_640x480;
   SiS_Pr->SiS_Panel800x600   = Panel_800x600;
   SiS_Pr->SiS_Panel1024x768  = Panel_1024x768;
   SiS_Pr->SiS_Panel1280x1024 = Panel_1280x1024;
   SiS_Pr->SiS_Panel1280x960  = Panel_1280x960;
   SiS_Pr->SiS_Panel1600x1200 = Panel_1600x1200;
   SiS_Pr->SiS_Panel1400x1050 = Panel_1400x1050;
   SiS_Pr->SiS_Panel1152x768  = Panel_1152x768;
   SiS_Pr->SiS_Panel1152x864  = Panel_1152x864;
   SiS_Pr->SiS_Panel1280x768  = Panel_1280x768;
   SiS_Pr->SiS_Panel1024x600  = Panel_1024x600;
   SiS_Pr->SiS_Panel640x480_2 = Panel_640x480_2;
   SiS_Pr->SiS_Panel640x480_3 = Panel_640x480_3;
   SiS_Pr->SiS_PanelMax       = Panel_320x480;    /* TW: highest value */
   SiS_Pr->SiS_PanelMinLVDS   = Panel_800x600;    /* TW: lowest value LVDS/LCDA */
   SiS_Pr->SiS_PanelMin301    = Panel_1024x768;   /* TW: lowest value 301 */
   SiS_Pr->SiS_PanelCustom    = Panel_Custom;
   SiS_Pr->SiS_PanelBarco1366 = 255;
}
#endif

#ifdef SIS315H
UCHAR
SiS_Get310DRAMType(SiS_Private *SiS_Pr, UCHAR *ROMAddr,PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
   UCHAR data, temp;

   if(*SiS_Pr->pSiS_SoftSetting & SoftDRAMType) {
     data = *SiS_Pr->pSiS_SoftSetting & 0x03;
   } else {
     if(IS_SIS550650740660) {
        data = SiS_GetReg1(SiS_Pr->SiS_P3c4,0x13) & 0x07;
     } else {	/* TW: 315, 330 */
        data = SiS_GetReg1(SiS_Pr->SiS_P3c4,0x3a) & 0x03;
        if(HwDeviceExtension->jChipType == SIS_330) {
	   if(data > 1) {
	      temp = SiS_GetReg1(SiS_Pr->SiS_P3d4,0x5f) & 0x30;
	      switch(temp) {
	      case 0x00: data = 1; break;
	      case 0x10: data = 3; break;
	      case 0x20: data = 3; break;
	      case 0x30: data = 2; break;
	      }
	   } else {
	      data = 0;
	   }
	}
     }
   }

   return data;
}
#endif

/* ----------------------------------------- */

void SiSRegInit(SiS_Private *SiS_Pr, USHORT BaseAddr)
{
   SiS_Pr->SiS_P3c4 = BaseAddr + 0x14;
   SiS_Pr->SiS_P3d4 = BaseAddr + 0x24;
   SiS_Pr->SiS_P3c0 = BaseAddr + 0x10;
   SiS_Pr->SiS_P3ce = BaseAddr + 0x1e;
   SiS_Pr->SiS_P3c2 = BaseAddr + 0x12;
   SiS_Pr->SiS_P3ca = BaseAddr + 0x1a;
   SiS_Pr->SiS_P3c6 = BaseAddr + 0x16;
   SiS_Pr->SiS_P3c7 = BaseAddr + 0x17;
   SiS_Pr->SiS_P3c8 = BaseAddr + 0x18;
   SiS_Pr->SiS_P3c9 = BaseAddr + 0x19;
   SiS_Pr->SiS_P3cb = BaseAddr + 0x1b;
   SiS_Pr->SiS_P3cd = BaseAddr + 0x1d;
   SiS_Pr->SiS_P3da = BaseAddr + 0x2a;
   SiS_Pr->SiS_Part1Port = BaseAddr + SIS_CRT2_PORT_04;     /* Digital video interface registers (LCD) */
   SiS_Pr->SiS_Part2Port = BaseAddr + SIS_CRT2_PORT_10;     /* 301 TV Encoder registers */
   SiS_Pr->SiS_Part3Port = BaseAddr + SIS_CRT2_PORT_12;     /* 301 Macrovision registers */
   SiS_Pr->SiS_Part4Port = BaseAddr + SIS_CRT2_PORT_14;     /* 301 VGA2 (and LCD) registers */
   SiS_Pr->SiS_Part5Port = BaseAddr + SIS_CRT2_PORT_14 + 2; /* 301 palette address port registers */
   SiS_Pr->SiS_DDC_Port = BaseAddr + 0x14;                  /* DDC Port ( = P3C4, SR11/0A) */
   SiS_Pr->SiS_VidCapt = BaseAddr + SIS_VIDEO_CAPTURE;
   SiS_Pr->SiS_VidPlay = BaseAddr + SIS_VIDEO_PLAYBACK;
}

void
SiSInitPCIetc(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
   switch(HwDeviceExtension->jChipType) {
   case SIS_300:
   case SIS_540:
   case SIS_630:
   case SIS_730:
      /* Set - PCI LINEAR ADDRESSING ENABLE (0x80)
       *     - RELOCATED VGA IO  (0x20)
       *     - MMIO ENABLE (0x1)
       */
      SiS_SetReg1(SiS_Pr->SiS_P3c4,0x20,0xa1);
      /*  - Enable 2D (0x40)
       *  - Enable 3D (0x02)
       *  - Enable 3D Vertex command fetch (0x10) ?
       *  - Enable 3D command parser (0x08) ?
       */
      SiS_SetRegOR(SiS_Pr->SiS_P3c4,0x1E,0x5A);
      break;
   case SIS_315H:
   case SIS_315:
   case SIS_315PRO:
   case SIS_650:
   case SIS_740:
   case SIS_330:
   case SIS_660:
   case SIS_760:
      SiS_SetReg1(SiS_Pr->SiS_P3c4,0x20,0xa1);
      /*  - Enable 2D (0x40)
       *  - Enable 3D (0x02)
       *  - Enable 3D vertex command fetch (0x10)
       *  - Enable 3D command parser (0x08)
       *  - Enable 3D G/L transformation engine (0x80)
       */
      SiS_SetRegOR(SiS_Pr->SiS_P3c4,0x1E,0xDA);
      break;
   case SIS_550:
      SiS_SetReg1(SiS_Pr->SiS_P3c4,0x20,0xa1);
      /* No 3D engine ! */
      /*  - Enable 2D (0x40)
       */
      SiS_SetRegOR(SiS_Pr->SiS_P3c4,0x1E,0x40);
   }
}

void
SiSSetLVDSetc(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension,USHORT ModeNo)
{
   ULONG   temp;

   SiS_Pr->SiS_IF_DEF_LVDS = 0;
   SiS_Pr->SiS_IF_DEF_TRUMPION = 0;
   SiS_Pr->SiS_IF_DEF_CH70xx = 0;
   SiS_Pr->SiS_IF_DEF_HiVision = 0;
   SiS_Pr->SiS_IF_DEF_DSTN = 0;
   SiS_Pr->SiS_IF_DEF_FSTN = 0;

   SiS_Pr->SiS_ChrontelInit = 0;

#if 0
   if(HwDeviceExtension->jChipType >= SIS_315H) {
      if((ModeNo == 0x5a) || (ModeNo == 0x5b)) {
   	 SiS_Pr->SiS_IF_DEF_DSTN = 1;   /* for 550 dstn */
   	 SiS_Pr->SiS_IF_DEF_FSTN = 1;   /* for fstn */
      }
   }
#endif

   switch(HwDeviceExtension->jChipType) {
#ifdef SIS300
   case SIS_540:
   case SIS_630:
   case SIS_730:
        /* Check for SiS30x first */
        temp = SiS_GetReg1(SiS_Pr->SiS_Part4Port,0x00);
	if((temp == 1) || (temp == 2)) return;
      	temp = SiS_GetReg1(SiS_Pr->SiS_P3d4,0x37);
      	temp = (temp & 0x0E) >> 1;
      	if((temp >= 2) && (temp <= 5)) SiS_Pr->SiS_IF_DEF_LVDS = 1;
      	if(temp == 3)   SiS_Pr->SiS_IF_DEF_TRUMPION = 1;
      	if((temp == 4) || (temp == 5)) {
		/* Save power status (and error check) - UNUSED */
		SiS_Pr->SiS_Backup70xx = SiS_GetCH700x(SiS_Pr, 0x0e);
		SiS_Pr->SiS_IF_DEF_CH70xx = 1;
        }
	break;
#endif
#ifdef SIS315H
   case SIS_550:
   case SIS_650:
   case SIS_740:
   case SIS_330:
   case SIS_660:
   case SIS_760:
	temp=SiS_GetReg1(SiS_Pr->SiS_P3d4,0x37);
      	temp = (temp & 0x0E) >> 1;
      	if((temp >= 2) && (temp <= 3)) SiS_Pr->SiS_IF_DEF_LVDS = 1;
      	if(temp == 3)  SiS_Pr->SiS_IF_DEF_CH70xx = 2;
        break;
#endif
   default:
        break;
   }
}

void
SiSInitPtr(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
   switch(HwDeviceExtension->jChipType) {
#ifdef SIS315H
   case SIS_315H:
   case SIS_315:
   case SIS_315PRO:
   case SIS_550:
   case SIS_650:
   case SIS_740:
   case SIS_330:
   case SIS_660:
   case SIS_760:
      InitTo310Pointer(SiS_Pr, HwDeviceExtension);
      break;
#endif
#ifdef SIS300
   case SIS_300:
   case SIS_540:
   case SIS_630:
   case SIS_730:
      InitTo300Pointer(SiS_Pr, HwDeviceExtension);
      break;
#endif
   default:
      break;
   }
}

void
SiSDetermineROMUsage(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension, UCHAR *ROMAddr)
{
   if((ROMAddr) && (HwDeviceExtension->UseROM)) {
     if((ROMAddr[0x00] != 0x55) || (ROMAddr[0x01] != 0xAA)) {
        SiS_Pr->SiS_UseROM = FALSE;
     } else if(HwDeviceExtension->jChipType == SIS_300) {
        /* TW: 300: We check if the code starts below 0x220 by
	 *     checking the jmp instruction at the beginning
	 *     of the BIOS image.
	 */
	 if((ROMAddr[3] == 0xe9) &&
	    ((ROMAddr[5] << 8) | ROMAddr[4]) > 0x21a)
	      SiS_Pr->SiS_UseROM = TRUE;
	 else SiS_Pr->SiS_UseROM = FALSE;
     } else if(HwDeviceExtension->jChipType < SIS_315H) {
#if 0
        /* TW: Rest of 300 series: We don't use the ROM image if
	 *     the BIOS version < 2.0.0 as such old BIOSes don't
	 *     have the needed data at the expected locations.
	 */
        if(ROMAddr[0x06] < '2')  SiS_Pr->SiS_UseROM = FALSE;
	else                     SiS_Pr->SiS_UseROM = TRUE;
#else
	/* Sony's VAIO BIOS 1.09 follows the standard, so perhaps
	 * the others do as well
	 */
	SiS_Pr->SiS_UseROM = TRUE;
#endif
     } else {
        /* TW: 315/330 series stick to the standard */
	SiS_Pr->SiS_UseROM = TRUE;
     }
   } else SiS_Pr->SiS_UseROM = FALSE;

}

/*
 	=========================================
 	======== SiS SetMode Functions ==========
 	=========================================
*/
#ifdef LINUX_XF86
/* TW: This is used for non-Dual-Head mode from X */
BOOLEAN
SiSBIOSSetMode(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension, ScrnInfoPtr pScrn,
               DisplayModePtr mode, BOOLEAN IsCustom)
{
   SISPtr  pSiS = SISPTR(pScrn);
   UShort  ModeNo=0;
   
   SiS_Pr->UseCustomMode = FALSE;

   if((IsCustom) && (SiS_CheckBuildCustomMode(pScrn, mode, pSiS->VBFlags))) {

         xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3, "Setting custom mode %dx%d\n",
	 	SiS_Pr->CHDisplay,
		(mode->Flags & V_INTERLACE ? SiS_Pr->CVDisplay * 2 :
		   (mode->Flags & V_DBLSCAN ? SiS_Pr->CVDisplay / 2 :
		      SiS_Pr->CVDisplay)));

	 return(SiSSetMode(SiS_Pr, HwDeviceExtension, pScrn, ModeNo, TRUE));

   }

   ModeNo = SiS_CalcModeIndex(pScrn, mode, pSiS->HaveCustomModes);
   if(!ModeNo) return FALSE;

   xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3, "Setting standard mode 0x%x\n", ModeNo);

   return(SiSSetMode(SiS_Pr, HwDeviceExtension, pScrn, ModeNo, TRUE));
}

#ifdef SISDUALHEAD
/* TW: Set CRT1 mode (used for dual head and MergedFB) */
BOOLEAN
SiSBIOSSetModeCRT1(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension, ScrnInfoPtr pScrn,
               DisplayModePtr mode, BOOLEAN IsCustom)
{
   ULONG   temp;
   USHORT  ModeIdIndex;
   UCHAR  *ROMAddr  = HwDeviceExtension->pjVirtualRomBase;
   USHORT  BaseAddr = (USHORT)HwDeviceExtension->ulIOAddress;
   SISPtr  pSiS = SISPTR(pScrn);
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
   unsigned char backupreg=0;
   BOOLEAN backupcustom;
   UShort  ModeNo=0;
   
   SiS_Pr->UseCustomMode = FALSE;

   if((IsCustom) && (SiS_CheckBuildCustomMode(pScrn, mode, pSiS->VBFlags))) {

         USHORT temptemp = SiS_Pr->CVDisplay;

         if(SiS_Pr->CModeFlag & DoubleScanMode)     temptemp >>= 1;
         else if(SiS_Pr->CInfoFlag & InterlaceMode) temptemp <<= 1;

         xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
	 	"Setting custom mode %dx%d on CRT1\n",
	 	SiS_Pr->CHDisplay, temptemp);
	 ModeNo = 0xfe;

   } else {

         ModeNo = SiS_CalcModeIndex(pScrn, mode, pSiS->HaveCustomModes);
         if(!ModeNo) return FALSE;

         xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
	 	"Setting standard mode 0x%x on CRT1\n", ModeNo);
   }

   SiSInitPtr(SiS_Pr, HwDeviceExtension);

   SiSRegInit(SiS_Pr, BaseAddr);

   SiS_GetSysFlags(SiS_Pr, HwDeviceExtension);

   SiS_Pr->SiS_VGAINFO = SiS_GetSetBIOSScratch(pScrn, 0x489, 0xff);

   SiSInitPCIetc(SiS_Pr, HwDeviceExtension);

   SiSSetLVDSetc(SiS_Pr, HwDeviceExtension, ModeNo);

   SiSDetermineROMUsage(SiS_Pr, HwDeviceExtension, ROMAddr);

   /* We don't clear the buffer under X */
   SiS_Pr->SiS_flag_clearbuffer = 0;

   /* 1.Openkey */
   SiS_SetReg1(SiS_Pr->SiS_P3c4,0x05,0x86);

   SiS_UnLockCRT2(SiS_Pr, HwDeviceExtension, BaseAddr);

   /* 2.Get ModeID Table  */
   if(!SiS_Pr->UseCustomMode) {
      temp = SiS_SearchModeID(SiS_Pr, ROMAddr,&ModeNo,&ModeIdIndex);
      if(temp == 0)  return(0);
   } else {
      ModeIdIndex = 0;
   }

   /* TW: Determine VBType (301,301B,301LV,302B,302LV) */
   SiS_GetVBType(SiS_Pr, BaseAddr,HwDeviceExtension);

   if(SiS_Pr->SiS_VBType & VB_SIS301BLV302BLV) {
      if(HwDeviceExtension->jChipType >= SIS_315H) {
         backupreg = SiS_GetReg1(SiS_Pr->SiS_P3d4,0x38);
      } else {
         backupreg = SiS_GetReg1(SiS_Pr->SiS_P3d4,0x35);
      }
   }

   /* TW: Get VB information (connectors, connected devices) */
   /* (We don't care if the current mode is a CRT2 mode) */
   SiS_GetVBInfo(SiS_Pr, BaseAddr,ROMAddr,ModeNo,ModeIdIndex,HwDeviceExtension,0);
   SiS_SetHiVision(SiS_Pr, BaseAddr,HwDeviceExtension);
   SiS_GetLCDResInfo(SiS_Pr, ROMAddr,ModeNo,ModeIdIndex,HwDeviceExtension);

   if(HwDeviceExtension->jChipType >= SIS_315H) {
      if(SiS_GetReg1(SiS_Pr->SiS_P3c4,0x17) & 0x08)  {
         if(SiS_Pr->SiS_IF_DEF_LVDS == 1) {
            if(ModeNo != 0x10)  SiS_Pr->SiS_SetFlag |= SetDOSMode;
         } else if((IS_SIS651) && (SiS_Pr->SiS_VBType & VB_NoLCD)) {
            SiS_Pr->SiS_SetFlag |= SetDOSMode;
         }
      }

      if(IS_SIS650) {
         if(SiS_Pr->SiS_VBType & VB_SIS301BLV302BLV) {
	    SiS_SetRegAND(SiS_Pr->SiS_P3d4,0x51,0x1f);
	    if(IS_SIS651) SiS_SetRegOR(SiS_Pr->SiS_P3d4,0x51,0x20);
	    SiS_SetRegAND(SiS_Pr->SiS_P3d4,0x56,0xe7);
	 }
      }
   }

   /* Set mode on CRT1 */
   SiS_SetCRT1Group(SiS_Pr, ROMAddr,HwDeviceExtension,ModeNo,ModeIdIndex,BaseAddr);

   /* SetPitch: Adapt to virtual size & position */
   SiS_SetPitchCRT1(SiS_Pr, pScrn, BaseAddr);

   if(pSiS->DualHeadMode) {
      pSiSEnt->CRT1ModeNo = ModeNo;
      pSiSEnt->CRT1DMode = mode;
   }

   if(SiS_Pr->UseCustomMode) {
      SiS_Pr->CRT1UsesCustomMode = TRUE;
      SiS_Pr->CSRClock_CRT1 = SiS_Pr->CSRClock;
      SiS_Pr->CModeFlag_CRT1 = SiS_Pr->CModeFlag;
   } else {
      SiS_Pr->CRT1UsesCustomMode = FALSE;
   }

   /* We have to reset CRT2 if changing mode on CRT1 */
   if(pSiS->DualHeadMode) {
      if(pSiSEnt->CRT2ModeNo != -1) {
         xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
				"(Re-)Setting mode for CRT2\n");
	 backupcustom = SiS_Pr->UseCustomMode;
	 SiSBIOSSetModeCRT2(SiS_Pr, HwDeviceExtension, pSiSEnt->pScrn_1,
			    pSiSEnt->CRT2DMode, pSiSEnt->CRT2IsCustom);
	 SiS_Pr->UseCustomMode = backupcustom;
      }
   }

   /* Warning: From here, the custom mode entries in SiS_Pr are
    * possibly overwritten
    */

   SiS_HandleCRT1(SiS_Pr);

   SiS_StrangeStuff(SiS_Pr, HwDeviceExtension);

   SiS_DisplayOn(SiS_Pr);
   SiS_SetReg3(SiS_Pr->SiS_P3c6,0xFF);

   if(SiS_Pr->SiS_VBType & VB_SIS301BLV302BLV) {
      if(HwDeviceExtension->jChipType >= SIS_315H) {
	 SiS_SetReg1(SiS_Pr->SiS_P3d4,0x38,backupreg);
      } else if((HwDeviceExtension->jChipType == SIS_630) ||
                (HwDeviceExtension->jChipType == SIS_730)) {
         SiS_SetReg1(SiS_Pr->SiS_P3d4,0x35,backupreg);
      }
   }

   /* Backup/Set ModeNo in BIOS scratch area */
   SiS_GetSetModeID(pScrn,ModeNo);

   return TRUE;
}

/* TW: Set CRT2 mode (used for dual head) */
BOOLEAN
SiSBIOSSetModeCRT2(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension, ScrnInfoPtr pScrn,
               DisplayModePtr mode, BOOLEAN IsCustom)
{
   ULONG   temp;
   USHORT  ModeIdIndex;
   UCHAR  *ROMAddr  = HwDeviceExtension->pjVirtualRomBase;
   USHORT  BaseAddr = (USHORT)HwDeviceExtension->ulIOAddress;
   UShort  ModeNo   = 0;
   SISPtr  pSiS     = SISPTR(pScrn);
   SISEntPtr pSiSEnt = pSiS->entityPrivate;
   unsigned char tempr1, tempr2, backupreg=0;

   SiS_Pr->UseCustomMode = FALSE;

   /* Remember: Custom modes for CRT2 are ONLY supported
    * 		-) on 315/330 series,
    *           -) on the 301 and 30xB, and
    *           -) if CRT2 is LCD or VGA
    */

   if((IsCustom) && (SiS_CheckBuildCustomMode(pScrn, mode, pSiS->VBFlags))) {

	 ModeNo = 0xfe;

   } else {

         BOOLEAN havecustommodes = pSiS->HaveCustomModes;

#ifdef SISMERGED
	 if(pSiS->MergedFB) havecustommodes = pSiS->HaveCustomModes2;
#endif

         ModeNo = SiS_CalcModeIndex(pScrn, mode, havecustommodes);
         if(!ModeNo) return FALSE;

   }

   /* Save mode info so we can set it from within SetMode for CRT1 */
   if(pSiS->DualHeadMode) {
      pSiSEnt->CRT2ModeNo = ModeNo;
      pSiSEnt->CRT2DMode = mode;
      pSiSEnt->CRT2IsCustom = IsCustom;

      /* We can't set CRT2 mode before CRT1 mode is set */
      if(pSiSEnt->CRT1ModeNo == -1) {
    	 xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
		"Setting CRT2 mode delayed until after setting CRT1 mode\n");
   	 return TRUE;
      }
   }

   SiSInitPtr(SiS_Pr, HwDeviceExtension);

   SiSRegInit(SiS_Pr, BaseAddr);

   SiS_GetSysFlags(SiS_Pr, HwDeviceExtension);

   SiS_Pr->SiS_VGAINFO = SiS_GetSetBIOSScratch(pScrn, 0x489, 0xff);

   SiSInitPCIetc(SiS_Pr, HwDeviceExtension);

   SiSSetLVDSetc(SiS_Pr, HwDeviceExtension, ModeNo);

   SiSDetermineROMUsage(SiS_Pr, HwDeviceExtension, ROMAddr);

   /* We don't clear the buffer under X */
   SiS_Pr->SiS_flag_clearbuffer=0;

   if(SiS_Pr->UseCustomMode) {

      USHORT temptemp = SiS_Pr->CVDisplay;

      if(SiS_Pr->CModeFlag & DoubleScanMode)     temptemp >>= 1;
      else if(SiS_Pr->CInfoFlag & InterlaceMode) temptemp <<= 1;

      xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
	  "Setting custom mode %dx%d on CRT2\n",
	  SiS_Pr->CHDisplay, temptemp);

   } else {

      xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3,
   	  "Setting standard mode 0x%x on CRT2\n", ModeNo);

   }

   /* 1.Openkey */
   SiS_SetReg1(SiS_Pr->SiS_P3c4,0x05,0x86);

   SiS_UnLockCRT2(SiS_Pr, HwDeviceExtension, BaseAddr);

   /* 2.Get ModeID */
   if(!SiS_Pr->UseCustomMode) {
      temp = SiS_SearchModeID(SiS_Pr, ROMAddr,&ModeNo,&ModeIdIndex);
      if(temp == 0)  return(0);
   } else {
      ModeIdIndex = 0;
   }

   /* Determine VBType (301,301B,301LV,302B,302LV) */
   SiS_GetVBType(SiS_Pr, BaseAddr,HwDeviceExtension);

   if(SiS_Pr->SiS_VBType & VB_SIS301BLV302BLV) {
      if(HwDeviceExtension->jChipType >= SIS_315H) {
         SiS_UnLockCRT2(SiS_Pr,HwDeviceExtension,BaseAddr);
	 if(HwDeviceExtension->jChipType < SIS_330) {
           if(ROMAddr && SiS_Pr->SiS_UseROM) {
             temp = ROMAddr[VB310Data_1_2_Offset];
	     temp |= 0x40;
             SiS_SetReg1(SiS_Pr->SiS_Part1Port,0x02,temp);
           }
	 }
	 SiS_SetRegOR(SiS_Pr->SiS_P3c4,0x32,0x10);

	 SiS_SetRegOR(SiS_Pr->SiS_Part2Port,0x02,0x0c);

         backupreg = SiS_GetReg1(SiS_Pr->SiS_P3d4,0x38);
      } else {
         backupreg = SiS_GetReg1(SiS_Pr->SiS_P3d4,0x35);
      }
   }

   /* Get VB information (connectors, connected devices) */
   if(!SiS_Pr->UseCustomMode) {
      SiS_GetVBInfo(SiS_Pr, BaseAddr,ROMAddr,ModeNo,ModeIdIndex,HwDeviceExtension,1);
   } else {
      /* If this is a custom mode, we don't check the modeflag for CRT2Mode */
      SiS_GetVBInfo(SiS_Pr, BaseAddr,ROMAddr,ModeNo,ModeIdIndex,HwDeviceExtension,0);
   }
   SiS_SetHiVision(SiS_Pr, BaseAddr,HwDeviceExtension);
   SiS_GetLCDResInfo(SiS_Pr, ROMAddr,ModeNo,ModeIdIndex,HwDeviceExtension);

   if(HwDeviceExtension->jChipType >= SIS_315H) {
      if(SiS_GetReg1(SiS_Pr->SiS_P3c4,0x17) & 0x08)  {
         if(SiS_Pr->SiS_IF_DEF_LVDS == 1) {
            if(ModeNo != 0x10)  SiS_Pr->SiS_SetFlag |= SetDOSMode;
         } else if((IS_SIS651) && (SiS_Pr->SiS_VBType & VB_NoLCD)) {
            SiS_Pr->SiS_SetFlag |= SetDOSMode;
         }
      }
   }

   /* Set mode on CRT2 */
   switch (HwDeviceExtension->ujVBChipID) {
     case VB_CHIP_301:
     case VB_CHIP_301B:
     case VB_CHIP_301C:
     case VB_CHIP_301LV:
     case VB_CHIP_302:
     case VB_CHIP_302B:
     case VB_CHIP_302LV:
        SiS_SetCRT2Group(SiS_Pr, BaseAddr,ROMAddr,ModeNo,HwDeviceExtension);
        break;
     case VB_CHIP_UNKNOWN:
        if(SiS_Pr->SiS_IF_DEF_LVDS     == 1 ||
	   SiS_Pr->SiS_IF_DEF_CH70xx   != 0 ||
	   SiS_Pr->SiS_IF_DEF_TRUMPION != 0) {
           SiS_SetCRT2Group(SiS_Pr,BaseAddr,ROMAddr,ModeNo,HwDeviceExtension);
  	}
        break;
   }

   SiS_StrangeStuff(SiS_Pr, HwDeviceExtension);

   SiS_DisplayOn(SiS_Pr);
   SiS_SetReg3(SiS_Pr->SiS_P3c6,0xFF);

   if(HwDeviceExtension->jChipType >= SIS_315H) {
      if(SiS_Pr->SiS_IF_DEF_LVDS == 1) {
         if(!(SiS_IsDualEdge(SiS_Pr, HwDeviceExtension, BaseAddr))) {
	     SiS_SetRegAND(SiS_Pr->SiS_Part1Port,0x13,0xfb);
	 }
      }
   }

   if(SiS_Pr->SiS_VBType & VB_SIS301BLV302BLV) {
      if(HwDeviceExtension->jChipType >= SIS_315H) {
	 if(SiS_IsVAMode(SiS_Pr,HwDeviceExtension, BaseAddr)) {
	     SiS_SetRegOR(SiS_Pr->SiS_P3d4,0x35,0x01);
	 } else {
	     SiS_SetRegAND(SiS_Pr->SiS_P3d4,0x35,0xFE);
	 }

	 SiS_SetReg1(SiS_Pr->SiS_P3d4,0x38,backupreg);

	 tempr1 = SiS_GetReg1(SiS_Pr->SiS_P3d4,0x30);
	 tempr2 = SiS_GetReg1(SiS_Pr->SiS_Part2Port,0x00);
	 if(tempr1 & SetCRT2ToAVIDEO) tempr2 &= 0xF7;
	 if(tempr1 & SetCRT2ToSVIDEO) tempr2 &= 0xFB;
	 SiS_SetReg1(SiS_Pr->SiS_Part2Port,0x00,tempr2);

	 if(tempr1 & SetCRT2ToLCD) {
	       SiS_SetRegAND(SiS_Pr->SiS_P3d4,0x38,0xfc);
	 }
      } else if((HwDeviceExtension->jChipType == SIS_630) ||
                (HwDeviceExtension->jChipType == SIS_730)) {
         SiS_SetReg1(SiS_Pr->SiS_P3d4,0x35,backupreg);
      }
   }

   /* SetPitch: Adapt to virtual size & position */
   SiS_SetPitchCRT2(SiS_Pr, pScrn, BaseAddr);

   return TRUE;
}
#endif /* Dualhead */
#endif /* Linux_XF86 */

#ifdef LINUX_XF86
/* TW: We need pScrn for setting the pitch correctly */
BOOLEAN
SiSSetMode(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension,ScrnInfoPtr pScrn,USHORT ModeNo, BOOLEAN dosetpitch)
#else
BOOLEAN
SiSSetMode(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension,USHORT ModeNo)
#endif
{
   ULONG   temp;
   USHORT  ModeIdIndex,KeepLockReg;
   UCHAR  *ROMAddr  = HwDeviceExtension->pjVirtualRomBase;
   USHORT  BaseAddr = (USHORT)HwDeviceExtension->ulIOAddress;
   unsigned char backupreg=0, tempr1, tempr2;

#ifndef LINUX_XF86
   SiS_Pr->UseCustomMode = FALSE;
   SiS_Pr->CRT1UsesCustomMode = FALSE;
#endif
   
   if(SiS_Pr->UseCustomMode) {
      ModeNo = 0xfe;
   }

   SiSInitPtr(SiS_Pr, HwDeviceExtension);

   SiSRegInit(SiS_Pr, BaseAddr);

   SiS_GetSysFlags(SiS_Pr, HwDeviceExtension);

#ifdef LINUX_XF86
   if(pScrn) SiS_Pr->SiS_VGAINFO = SiS_GetSetBIOSScratch(pScrn, 0x489, 0xff);
   else
#endif
         SiS_Pr->SiS_VGAINFO = 0x11;

#ifdef LINUX_XF86
#ifdef TWDEBUG
   xf86DrvMsg(0, X_INFO, "VGAInfo 0x%02x\n", SiS_Pr->SiS_VGAINFO);
#endif
#endif

   SiSInitPCIetc(SiS_Pr, HwDeviceExtension);

   SiSSetLVDSetc(SiS_Pr, HwDeviceExtension, ModeNo);

   SiSDetermineROMUsage(SiS_Pr, HwDeviceExtension, ROMAddr);

   if(!SiS_Pr->UseCustomMode) {
      /* TW: Shift the clear-buffer-bit away */
      ModeNo = ((ModeNo & 0x80) << 8) | (ModeNo & 0x7f);
   }

#ifdef LINUX_XF86
   /* We never clear the buffer in X */
   ModeNo |= 0x8000;
#endif

   if(ModeNo & 0x8000) {
     	ModeNo &= 0x7fff;
     	SiS_Pr->SiS_flag_clearbuffer = 0;
   } else {
     	SiS_Pr->SiS_flag_clearbuffer = 1;
   }

   /* 1.Openkey */
   KeepLockReg = SiS_GetReg1(SiS_Pr->SiS_P3c4,0x05);
   SiS_SetReg1(SiS_Pr->SiS_P3c4,0x05,0x86);

   SiS_UnLockCRT2(SiS_Pr, HwDeviceExtension, BaseAddr);

   if(!SiS_Pr->UseCustomMode) {

      /* 2.Get ModeID Table  */
      temp = SiS_SearchModeID(SiS_Pr,ROMAddr,&ModeNo,&ModeIdIndex);
      if(temp == 0) return(0);

   } else {

      ModeIdIndex = 0;

   }

   /* Determine VBType (301,301B,301LV,302B,302LV) */
   SiS_GetVBType(SiS_Pr,BaseAddr,HwDeviceExtension);

   /* Init/restore some VB registers */
   if(SiS_Pr->SiS_VBType & VB_SIS301BLV302BLV) {
       if(HwDeviceExtension->jChipType >= SIS_315H) {
         SiS_UnLockCRT2(SiS_Pr,HwDeviceExtension,BaseAddr);
	 if(HwDeviceExtension->jChipType < SIS_330) {
           if(ROMAddr && SiS_Pr->SiS_UseROM) {
             temp = ROMAddr[VB310Data_1_2_Offset];
	     temp |= 0x40;
             SiS_SetReg1(SiS_Pr->SiS_Part1Port,0x02,temp);
           }
	 }
	 SiS_SetRegOR(SiS_Pr->SiS_P3c4,0x32,0x10);

	 SiS_SetRegOR(SiS_Pr->SiS_Part2Port,0x02,0x0c);

         backupreg = SiS_GetReg1(SiS_Pr->SiS_P3d4,0x38);
       } else {
         backupreg = SiS_GetReg1(SiS_Pr->SiS_P3d4,0x35);
       }
   }
   
   /* Get VB information (connectors, connected devices) */
   if(SiS_Pr->UseCustomMode) {
      SiS_GetVBInfo(SiS_Pr,BaseAddr,ROMAddr,ModeNo,ModeIdIndex,HwDeviceExtension,0);
   } else {
      SiS_GetVBInfo(SiS_Pr,BaseAddr,ROMAddr,ModeNo,ModeIdIndex,HwDeviceExtension,1);
   }
   SiS_SetHiVision(SiS_Pr,BaseAddr,HwDeviceExtension);
   SiS_GetLCDResInfo(SiS_Pr,ROMAddr,ModeNo,ModeIdIndex,HwDeviceExtension);

#ifndef LINUX_XF86
   /* 3. Check memory size (Kernel framebuffer driver only) */
   temp = SiS_CheckMemorySize(SiS_Pr,ROMAddr,HwDeviceExtension,ModeNo,ModeIdIndex);
   if(!temp) return(0);
#endif

   if(HwDeviceExtension->jChipType >= SIS_315H) {
      if(SiS_GetReg1(SiS_Pr->SiS_P3c4,0x17) & 0x08)  {
         if(SiS_Pr->SiS_IF_DEF_LVDS == 1) {
            if(ModeNo != 0x10)  SiS_Pr->SiS_SetFlag |= SetDOSMode;
         } else if((IS_SIS651) && (SiS_Pr->SiS_VBType & VB_NoLCD)) {
            SiS_Pr->SiS_SetFlag |= SetDOSMode;
         }
      }

      if(IS_SIS650) {
         if(SiS_Pr->SiS_VBType & VB_SIS301BLV302BLV) {
	    SiS_SetRegAND(SiS_Pr->SiS_P3d4,0x51,0x1f);
	    if(IS_SIS651) SiS_SetRegOR(SiS_Pr->SiS_P3d4,0x51,0x20);
	    SiS_SetRegAND(SiS_Pr->SiS_P3d4,0x56,0xe7);
	 }
      }
   }

   if(SiS_Pr->UseCustomMode) {
      SiS_Pr->CRT1UsesCustomMode = TRUE;
      SiS_Pr->CSRClock_CRT1 = SiS_Pr->CSRClock;
      SiS_Pr->CModeFlag_CRT1 = SiS_Pr->CModeFlag;
   } else {
      SiS_Pr->CRT1UsesCustomMode = FALSE;
   }

   /* Set mode on CRT1 */
   if(SiS_Pr->SiS_VBInfo & (SetSimuScanMode | SetCRT2ToLCDA)) {
   	SiS_SetCRT1Group(SiS_Pr,ROMAddr,HwDeviceExtension,ModeNo,ModeIdIndex,BaseAddr);
   } else {
     if(!(SiS_Pr->SiS_VBInfo & SwitchToCRT2)) {
       	SiS_SetCRT1Group(SiS_Pr,ROMAddr,HwDeviceExtension,ModeNo,ModeIdIndex,BaseAddr);
     }
   }

   /* Set mode on CRT2 */
   if(SiS_Pr->SiS_VBInfo & (SetSimuScanMode | SwitchToCRT2 | SetCRT2ToLCDA)) {
     switch (HwDeviceExtension->ujVBChipID) {
     case VB_CHIP_301:
     case VB_CHIP_301B:
     case VB_CHIP_301C:
     case VB_CHIP_301LV:
     case VB_CHIP_302:
     case VB_CHIP_302B:
     case VB_CHIP_302LV:
        SiS_SetCRT2Group(SiS_Pr,BaseAddr,ROMAddr,ModeNo,HwDeviceExtension);
        break;
     case VB_CHIP_UNKNOWN:
	if(SiS_Pr->SiS_IF_DEF_LVDS     == 1 ||
	   SiS_Pr->SiS_IF_DEF_CH70xx   != 0 ||
	   SiS_Pr->SiS_IF_DEF_TRUMPION != 0)
           SiS_SetCRT2Group(SiS_Pr,BaseAddr,ROMAddr,ModeNo,HwDeviceExtension);
        break;
     }
   }
   
   SiS_HandleCRT1(SiS_Pr);

   SiS_StrangeStuff(SiS_Pr, HwDeviceExtension);
   
   SiS_DisplayOn(SiS_Pr);
   SiS_SetReg3(SiS_Pr->SiS_P3c6,0xFF);

   if(HwDeviceExtension->jChipType >= SIS_315H) {
      if(SiS_Pr->SiS_IF_DEF_LVDS == 1) {
         if(!(SiS_IsDualEdge(SiS_Pr, HwDeviceExtension, BaseAddr))) {
	     SiS_SetRegAND(SiS_Pr->SiS_Part1Port,0x13,0xfb);
	 }
      }
   }

   if(SiS_Pr->SiS_VBType & VB_SIS301BLV302BLV) {
      if(HwDeviceExtension->jChipType >= SIS_315H) {
	 if(SiS_IsVAMode(SiS_Pr,HwDeviceExtension, BaseAddr)) {
	     SiS_SetRegOR(SiS_Pr->SiS_P3d4,0x35,0x01);
	 } else {
	     SiS_SetRegAND(SiS_Pr->SiS_P3d4,0x35,0xFE);
	 }

	 SiS_SetReg1(SiS_Pr->SiS_P3d4,0x38,backupreg);

	 tempr1 = SiS_GetReg1(SiS_Pr->SiS_P3d4,0x30);
	 tempr2 = SiS_GetReg1(SiS_Pr->SiS_Part2Port,0x00);
	 if(tempr1 & SetCRT2ToAVIDEO) tempr2 &= 0xF7;
	 if(tempr1 & SetCRT2ToSVIDEO) tempr2 &= 0xFB;
	 SiS_SetReg1(SiS_Pr->SiS_Part2Port,0x00,tempr2);

	 if((IS_SIS650) && (SiS_GetReg1(SiS_Pr->SiS_P3d4,0x30) & 0xfc)) {
	    if((ModeNo == 0x03) || (ModeNo == 0x10)) {
	        SiS_SetRegOR(SiS_Pr->SiS_P3d4,0x51,0x80);
	        SiS_SetRegOR(SiS_Pr->SiS_P3d4,0x56,0x08);
            }
	 }

	 if(tempr1 & SetCRT2ToLCD) {
	       SiS_SetRegAND(SiS_Pr->SiS_P3d4,0x38,0xfc);
	 }
      } else if((HwDeviceExtension->jChipType == SIS_630) ||
                (HwDeviceExtension->jChipType == SIS_730)) {
         SiS_SetReg1(SiS_Pr->SiS_P3d4,0x35,backupreg);
      }
   }

#ifdef LINUX_XF86
   if(pScrn) {
      /* SetPitch: Adapt to virtual size & position */
      if((ModeNo > 0x13) && (dosetpitch)) {
         SiS_SetPitch(SiS_Pr, pScrn, BaseAddr);
      }

      /* Backup/Set ModeNo in BIOS scratch area */
      SiS_GetSetModeID(pScrn, ModeNo);
   }
#endif

#ifndef LINUX_XF86  /* We never lock registers in XF86 */
   if(KeepLockReg == 0xA1) SiS_SetReg1(SiS_Pr->SiS_P3c4,0x05,0x86);
   else SiS_SetReg1(SiS_Pr->SiS_P3c4,0x05,0x00);
#endif

   return TRUE;
}

void
SiS_SetEnableDstn(SiS_Private *SiS_Pr, int enable)
{
   SiS_Pr->SiS_IF_DEF_DSTN = enable ? 1 : 0;
}

void
SiS_SetEnableFstn(SiS_Private *SiS_Pr, int enable)
{
   SiS_Pr->SiS_IF_DEF_FSTN = enable ? 1 : 0;
}

void
SiS_HandleCRT1(SiS_Private *SiS_Pr)
{
  /* TW: We don't do this at all. There is a new
   * CRT1-is-connected-at-boot-time logic in the 650 BIOS, which
   * confuses our own. So just clear the bit and skip the rest.
   */

  SiS_SetRegAND(SiS_Pr->SiS_P3d4,0x63,0xbf);

#if 0
  if(!(SiS_GetReg1(SiS_Pr->SiS_P3c4,0x15) & 0x01)) {
     if((SiS_GetReg1(SiS_Pr->SiS_P3c4,0x15) & 0x0a) ||
        (SiS_GetReg1(SiS_Pr->SiS_P3c4,0x16) & 0x01)) {
        SiS_SetRegOR(SiS_Pr->SiS_P3d4,0x63,0x40);
     }
  }
#endif
}

void
SiS_GetSysFlags(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
   unsigned char cr5f, temp1, temp2;

   /* You should use the macros, not these flags directly */

   SiS_Pr->SiS_SysFlags = 0;
   if(HwDeviceExtension->jChipType == SIS_650) {
      cr5f = SiS_GetReg1(SiS_Pr->SiS_P3d4,0x5f) & 0xf0;
      SiS_SetRegAND(SiS_Pr->SiS_P3d4,0x5c,0x07);
      temp1 = SiS_GetReg1(SiS_Pr->SiS_P3d4,0x5c) & 0xf8;
      SiS_SetRegOR(SiS_Pr->SiS_P3d4,0x5c,0xf8);
      temp2 = SiS_GetReg1(SiS_Pr->SiS_P3d4,0x5c) & 0xf8;
      if((!temp1) || (temp2)) {
         switch(cr5f) {
	    case 0x80:
	    case 0x90:
	    case 0xc0:
	       SiS_Pr->SiS_SysFlags |= SF_IsM650;  break;
	    case 0xa0:
	    case 0xb0:
	    case 0xe0:
	       SiS_Pr->SiS_SysFlags |= SF_Is651;   break;
	 }
      } else {
         switch(cr5f) {
	    case 0x90:
	       temp1 = SiS_GetReg1(SiS_Pr->SiS_P3d4,0x5c) & 0xf8;
	       switch(temp1) {
	          case 0x00: SiS_Pr->SiS_SysFlags |= SF_IsM652; break;
		  case 0x40: SiS_Pr->SiS_SysFlags |= SF_IsM653; break;
		  default:   SiS_Pr->SiS_SysFlags |= SF_IsM650; break;
	       }
	       break;
	    case 0xb0:
	       SiS_Pr->SiS_SysFlags |= SF_Is652;  break;
	    default:
	       SiS_Pr->SiS_SysFlags |= SF_IsM650; break;
	 }
      }
   }
}

void
SiS_StrangeStuff(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
   if((IS_SIS651) || (IS_SISM650)) {
      SiS_SetReg1(SiS_Pr->SiS_VidCapt, 0x3f, 0x00);   /* Fiddle with capture regs */
      SiS_SetReg1(SiS_Pr->SiS_VidCapt, 0x00, 0x00);
      SiS_SetReg1(SiS_Pr->SiS_VidPlay, 0x00, 0x86);   /* (BIOS does NOT unlock) */
      SiS_SetRegAND(SiS_Pr->SiS_VidPlay, 0x30, 0xfe); /* Fiddle with video regs */
      SiS_SetRegAND(SiS_Pr->SiS_VidPlay, 0x3f, 0xef);
   }
   /* !!! This does not support modes < 0x13 !!! */
}

void
SiS_SetCRT1Group(SiS_Private *SiS_Pr, UCHAR *ROMAddr,PSIS_HW_DEVICE_INFO HwDeviceExtension,
                 USHORT ModeNo,USHORT ModeIdIndex,USHORT BaseAddr)
{
  USHORT  StandTableIndex,RefreshRateTableIndex;

  SiS_Pr->SiS_CRT1Mode = ModeNo;
  StandTableIndex = SiS_GetModePtr(SiS_Pr,ROMAddr,ModeNo,ModeIdIndex);
  if(SiS_LowModeStuff(SiS_Pr,ModeNo,HwDeviceExtension)) {
    if(SiS_Pr->SiS_VBInfo & (SetSimuScanMode | SwitchToCRT2)) {
       SiS_DisableBridge(SiS_Pr,HwDeviceExtension,BaseAddr);
    }
  }

  /* 550, 651 */
  SiS_WhatTheHellIsThis(SiS_Pr,HwDeviceExtension,BaseAddr);

  SiS_SetSeqRegs(SiS_Pr,ROMAddr,StandTableIndex);
  SiS_SetMiscRegs(SiS_Pr,ROMAddr,StandTableIndex);
  SiS_SetCRTCRegs(SiS_Pr,ROMAddr,HwDeviceExtension,StandTableIndex);
  SiS_SetATTRegs(SiS_Pr,ROMAddr,StandTableIndex,HwDeviceExtension);
  SiS_SetGRCRegs(SiS_Pr,ROMAddr,StandTableIndex);
  SiS_ClearExt1Regs(SiS_Pr,HwDeviceExtension);
  SiS_ResetCRT1VCLK(SiS_Pr,ROMAddr,HwDeviceExtension);

  SiS_Pr->SiS_SelectCRT2Rate = 0;
  SiS_Pr->SiS_SetFlag &= (~ProgrammingCRT2);

#ifdef LINUX_XF86
  xf86DrvMsgVerb(0, X_PROBED, 3, "(init: VBType=0x%04x, VBInfo=0x%04x)\n",
                    SiS_Pr->SiS_VBType, SiS_Pr->SiS_VBInfo);
#endif

  if(SiS_Pr->SiS_VBInfo & SetSimuScanMode) {
     if(SiS_Pr->SiS_VBInfo & SetInSlaveMode) {
        SiS_Pr->SiS_SetFlag |= ProgrammingCRT2;
     }
  }

  if(SiS_Pr->SiS_VBInfo & SetCRT2ToLCDA) {
	SiS_Pr->SiS_SetFlag |= ProgrammingCRT2;
  }

  RefreshRateTableIndex = SiS_GetRatePtrCRT2(SiS_Pr,ROMAddr,ModeNo,ModeIdIndex,HwDeviceExtension);

  if(!(SiS_Pr->SiS_VBInfo & SetCRT2ToLCDA)) {
	SiS_Pr->SiS_SetFlag &= ~ProgrammingCRT2;
  }

  if(RefreshRateTableIndex != 0xFFFF) {
    	SiS_SetSync(SiS_Pr,ROMAddr,RefreshRateTableIndex);
    	SiS_SetCRT1CRTC(SiS_Pr,ROMAddr,ModeNo,ModeIdIndex,RefreshRateTableIndex,HwDeviceExtension);
    	SiS_SetCRT1Offset(SiS_Pr,ROMAddr,ModeNo,ModeIdIndex,RefreshRateTableIndex,HwDeviceExtension);
    	SiS_SetCRT1VCLK(SiS_Pr,ROMAddr,ModeNo,ModeIdIndex,HwDeviceExtension,RefreshRateTableIndex);
  }

#ifdef SIS300
  if(HwDeviceExtension->jChipType == SIS_300) {
     	SiS_SetCRT1FIFO_300(SiS_Pr,ROMAddr,ModeNo,HwDeviceExtension,RefreshRateTableIndex);
  }
  if((HwDeviceExtension->jChipType == SIS_630) ||
     (HwDeviceExtension->jChipType == SIS_730) ||
     (HwDeviceExtension->jChipType == SIS_540)) {
     	SiS_SetCRT1FIFO_630(SiS_Pr,ROMAddr,ModeNo,HwDeviceExtension,RefreshRateTableIndex);
  }
#endif
#ifdef SIS315H
  if(HwDeviceExtension->jChipType >= SIS_315H) {
     	SiS_SetCRT1FIFO_310(SiS_Pr,ROMAddr,ModeNo,ModeIdIndex,HwDeviceExtension);
  }
#endif

  SiS_SetCRT1ModeRegs(SiS_Pr,ROMAddr,HwDeviceExtension,ModeNo,ModeIdIndex,RefreshRateTableIndex);

  SiS_LoadDAC(SiS_Pr,HwDeviceExtension,ROMAddr,ModeNo,ModeIdIndex);

#ifndef LINUX_XF86
  if(SiS_Pr->SiS_flag_clearbuffer) {
        SiS_ClearBuffer(SiS_Pr,HwDeviceExtension,ModeNo);
  }
#endif

  if(!(SiS_Pr->SiS_VBInfo & (SetSimuScanMode | SwitchToCRT2 | SetCRT2ToLCDA))) {
        SiS_LongWait(SiS_Pr);
        SiS_DisplayOn(SiS_Pr);
  }
}

#ifdef LINUX_XF86
void
SiS_SetPitch(SiS_Private *SiS_Pr, ScrnInfoPtr pScrn, UShort BaseAddr)
{
   SISPtr pSiS = SISPTR(pScrn);
   BOOLEAN isslavemode = FALSE;

   if( (pSiS->VBFlags & VB_VIDEOBRIDGE) &&
       ( ((pSiS->VGAEngine == SIS_300_VGA) && (SiS_GetReg1(SiS_Pr->SiS_Part1Port,0x00) & 0xa0) == 0x20) ||
         ((pSiS->VGAEngine == SIS_315_VGA) && (SiS_GetReg1(SiS_Pr->SiS_Part1Port,0x00) & 0x50) == 0x10) ) ) {
      isslavemode = TRUE;
   }

   /* We need to set pitch for CRT1 if bridge is in slave mode, too */
   if( (pSiS->VBFlags & DISPTYPE_DISP1) || (isslavemode) ) {
   	SiS_SetPitchCRT1(SiS_Pr, pScrn, BaseAddr);
   }
   /* We must not set the pitch for CRT2 if bridge is in slave mode */
   if( (pSiS->VBFlags & DISPTYPE_DISP2) && (!isslavemode) ) {
   	SiS_SetPitchCRT2(SiS_Pr, pScrn, BaseAddr);
   }
}

void
SiS_SetPitchCRT1(SiS_Private *SiS_Pr, ScrnInfoPtr pScrn, UShort BaseAddr)
{
    SISPtr pSiS = SISPTR(pScrn);
    ULong  HDisplay,temp;

    HDisplay = pSiS->scrnPitch / 8;
    SiS_SetReg1(SiS_Pr->SiS_P3d4, 0x13, (HDisplay & 0xFF));
    temp = (SiS_GetReg1(SiS_Pr->SiS_P3c4, 0x0E) & 0xF0) | (HDisplay>>8);
    SiS_SetReg1(SiS_Pr->SiS_P3c4, 0x0E, temp);
}

void
SiS_SetPitchCRT2(SiS_Private *SiS_Pr, ScrnInfoPtr pScrn, UShort BaseAddr)
{
    SISPtr pSiS = SISPTR(pScrn);
    ULong  HDisplay,temp;

    HDisplay = pSiS->scrnPitch2 / 8;

    /* Unlock CRT2 */
    if (pSiS->VGAEngine == SIS_315_VGA)
        SiS_SetRegOR(SiS_Pr->SiS_Part1Port,0x2F, 0x01);
    else
        SiS_SetRegOR(SiS_Pr->SiS_Part1Port,0x24, 0x01);

    SiS_SetReg1(SiS_Pr->SiS_Part1Port,0x07, (HDisplay & 0xFF));
    temp = (SiS_GetReg1(SiS_Pr->SiS_Part1Port,0x09) & 0xF0) | ((HDisplay >> 8) & 0xFF);
    SiS_SetReg1(SiS_Pr->SiS_Part1Port,0x09, temp);
}
#endif

void
SiS_GetVBType(SiS_Private *SiS_Pr, USHORT BaseAddr,PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
  USHORT flag=0, rev=0, nolcd=0;

  SiS_Pr->SiS_VBType = 0;

  if(SiS_Pr->SiS_IF_DEF_LVDS == 1) return;

  flag = SiS_GetReg1(SiS_Pr->SiS_Part4Port,0x00);

  if(flag > 3) return;

  rev = SiS_GetReg1(SiS_Pr->SiS_Part4Port,0x01);

  if(flag >= 2) {
        SiS_Pr->SiS_VBType = VB_SIS302B;
  } else if(flag == 1) {
        SiS_Pr->SiS_VBType = VB_SIS301;
	if(rev >= 0xC0) {
            	SiS_Pr->SiS_VBType = VB_SIS301C;
        } else if(rev >= 0xB0) {
            	SiS_Pr->SiS_VBType = VB_SIS301B;
		/* Check if 30xB DH version (no LCD support, use Panel Link instead) */
    		nolcd = SiS_GetReg1(SiS_Pr->SiS_Part4Port,0x23);
                if(!(nolcd & 0x02)) SiS_Pr->SiS_VBType |= VB_NoLCD;
        }
  }
  if(SiS_Pr->SiS_VBType & (VB_SIS301B | VB_SIS301C | VB_SIS302B)) {
        if(rev >= 0xD0) {
	        SiS_Pr->SiS_VBType &= ~(VB_SIS301B | VB_SIS301C | VB_SIS302B);
          	SiS_Pr->SiS_VBType |= VB_SIS301LV;
		SiS_Pr->SiS_VBType &= ~(VB_NoLCD);
		if(rev >= 0xE0) {
		    SiS_Pr->SiS_VBType &= ~(VB_SIS301LV);
		    SiS_Pr->SiS_VBType |= VB_SIS302LV;
		}
        }
  }
}

BOOLEAN
SiS_SearchModeID(SiS_Private *SiS_Pr, UCHAR *ROMAddr,USHORT *ModeNo,USHORT *ModeIdIndex)
{
   UCHAR VGAINFO = SiS_Pr->SiS_VGAINFO;

   if(*ModeNo <= 0x13) {

      if((*ModeNo) <= 0x05) (*ModeNo) |= 0x01;

      for(*ModeIdIndex = 0; ;(*ModeIdIndex)++) {
         if(SiS_Pr->SiS_SModeIDTable[*ModeIdIndex].St_ModeID == (*ModeNo)) break;
         if(SiS_Pr->SiS_SModeIDTable[*ModeIdIndex].St_ModeID == 0xFF)   return FALSE;
      }

      if(*ModeNo == 0x07) {
          if(VGAINFO & 0x10) (*ModeIdIndex)++;   /* 400 lines */
          /* else 350 lines */
      }
      if(*ModeNo <= 0x03) {
         if(!(VGAINFO & 0x80)) (*ModeIdIndex)++;
         if(VGAINFO & 0x10)    (*ModeIdIndex)++; /* 400 lines  */
         /* else 350 lines  */
      }
      /* else 200 lines  */

   } else {

      for(*ModeIdIndex = 0; ;(*ModeIdIndex)++) {
         if(SiS_Pr->SiS_EModeIDTable[*ModeIdIndex].Ext_ModeID == (*ModeNo)) break;
         if(SiS_Pr->SiS_EModeIDTable[*ModeIdIndex].Ext_ModeID == 0xFF)      return FALSE;
      }

   }
   return TRUE;
}

BOOLEAN
SiS_SearchVBModeID(SiS_Private *SiS_Pr, UCHAR *ROMAddr, USHORT *ModeNo)
{
   USHORT ModeIdIndex;
   UCHAR VGAINFO = SiS_Pr->SiS_VGAINFO;

   if(*ModeNo <= 5) *ModeNo |= 1;

   for(ModeIdIndex=0; ; ModeIdIndex++) {
        if(SiS_Pr->SiS_VBModeIDTable[ModeIdIndex].ModeID == *ModeNo) break;
        if(SiS_Pr->SiS_VBModeIDTable[ModeIdIndex].ModeID == 0xFF)    return FALSE;
   }

   if(*ModeNo != 0x07) {
        if(*ModeNo > 0x03) return ((BOOLEAN)ModeIdIndex);
	if(VGAINFO & 0x80) return ((BOOLEAN)ModeIdIndex);
	ModeIdIndex++;
   }
   if(VGAINFO & 0x10) ModeIdIndex++;   /* 400 lines */
	                               /* else 350 lines */
   return ((BOOLEAN)ModeIdIndex);
}

#ifndef LINUX_XF86
BOOLEAN
SiS_CheckMemorySize(SiS_Private *SiS_Pr, UCHAR *ROMAddr,PSIS_HW_DEVICE_INFO HwDeviceExtension,
                    USHORT ModeNo,USHORT ModeIdIndex)
{
  USHORT memorysize,modeflag;
  ULONG  temp;

  if(SiS_Pr->UseCustomMode) {
     modeflag = SiS_Pr->CModeFlag;
  } else {
     if(ModeNo <= 0x13) {
        modeflag = SiS_Pr->SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;
     } else {
        modeflag = SiS_Pr->SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;
     }
  }

  memorysize = modeflag & MemoryInfoFlag;
  memorysize >>= MemorySizeShift;			/* Get required memory size */
  memorysize++;

  temp = GetDRAMSize(SiS_Pr, HwDeviceExtension);       	/* Get adapter memory size */
  temp /= (1024*1024);   				/* (in MB) */

  if(temp < memorysize) return(FALSE);
  else return(TRUE);
}
#endif

UCHAR
SiS_GetModePtr(SiS_Private *SiS_Pr, UCHAR *ROMAddr,USHORT ModeNo,USHORT ModeIdIndex)
{
   UCHAR index;

   if(ModeNo <= 0x13) {
     	index = SiS_Pr->SiS_SModeIDTable[ModeIdIndex].St_StTableIndex;
   } else {
     	if(SiS_Pr->SiS_ModeType <= 0x02) index = 0x1B;    /* 02 -> ModeEGA  */
     	else index = 0x0F;
   }
   return index;
}

static void
SiS_WhatIsThis1a(SiS_Private *SiS_Pr, USHORT somevalue)
{
   USHORT temp, tempbl, tempbh;

   tempbl = tempbh = somevalue;
   temp = SiS_GetReg2(SiS_Pr->SiS_P3cb);
   temp &= 0xf0;
   tempbl >>= 4;
   temp |= tempbl;
   SiS_SetReg3(SiS_Pr->SiS_P3cb, temp);
   temp = SiS_GetReg2(SiS_Pr->SiS_P3cd);
   temp &= 0xf0;
   tempbh &= 0x0f;
   temp |= tempbh;
   SiS_SetReg3(SiS_Pr->SiS_P3cd, temp);
}

static void
SiS_WhatIsThis1b(SiS_Private *SiS_Pr, USHORT somevalue)
{
   USHORT temp, tempbl, tempbh;

   tempbl = tempbh = somevalue;
   temp = SiS_GetReg2(SiS_Pr->SiS_P3cb);
   temp &= 0x0f;
   tempbl &= 0xf0;
   temp |= tempbl;
   SiS_SetReg3(SiS_Pr->SiS_P3cb, temp);
   temp = SiS_GetReg2(SiS_Pr->SiS_P3cd);
   temp &= 0x0f;
   tempbh <<= 4;
   temp |= tempbh;
   SiS_SetReg3(SiS_Pr->SiS_P3cd, temp);
}

static void
SiS_WhatIsThis2b(SiS_Private *SiS_Pr, USHORT somevalue)
{
   SiS_WhatIsThis1a(SiS_Pr, somevalue);
   SiS_WhatIsThis1b(SiS_Pr, somevalue);
}

static void
SiS_WhatIsThis1(SiS_Private *SiS_Pr)
{
   SiS_WhatIsThis2b(SiS_Pr, 0);
}

static void
SiS_WhatIsThis2a(SiS_Private *SiS_Pr, USHORT somevalue)
{
   USHORT temp = somevalue >> 8;

   temp &= 0x07;
   temp |= (temp << 4);
   SiS_SetReg1(SiS_Pr->SiS_P3c4,0x1d,temp);
   SiS_WhatIsThis2b(SiS_Pr, somevalue);
}

static void
SiS_WhatIsThis2(SiS_Private *SiS_Pr)
{
   SiS_WhatIsThis2a(SiS_Pr, 0);
}

void
SiS_WhatTheHellIsThis(SiS_Private *SiS_Pr,PSIS_HW_DEVICE_INFO HwDeviceExtension,USHORT BaseAddr)
{
   if(IS_SIS65x) {
      SiS_WhatIsThis1(SiS_Pr);
      SiS_WhatIsThis2(SiS_Pr);
   }
}

void
SiS_SetSeqRegs(SiS_Private *SiS_Pr, UCHAR *ROMAddr,USHORT StandTableIndex)
{
   UCHAR SRdata;
   USHORT i;

   SiS_SetReg1(SiS_Pr->SiS_P3c4,0x00,0x03);           	/* Set SR0  */

   SRdata = SiS_Pr->SiS_StandTable[StandTableIndex].SR[0];

   if(SiS_Pr->SiS_VBType & VB_SIS301BLV302BLV) {
      if(SiS_Pr->SiS_VBInfo & SetCRT2ToLCDA) {
         SRdata |= 0x01;
      }
      if(SiS_Pr->SiS_VBInfo & SetCRT2ToLCD) {
         if(SiS_Pr->SiS_VBType & VB_NoLCD) {
	    if(SiS_Pr->SiS_VBInfo & SetInSlaveMode) {
               SRdata |= 0x01;          		/* 8 dot clock  */
            }
	 }
      }
   }
   if(SiS_Pr->SiS_IF_DEF_LVDS == 1) {
     if(SiS_Pr->SiS_IF_DEF_CH70xx != 0) {
       if(SiS_Pr->SiS_VBInfo & SetCRT2ToTV) {
         if(SiS_Pr->SiS_VBInfo & SetInSlaveMode) {
           SRdata |= 0x01;        			/* 8 dot clock  */
         }
       }
     }
     if(SiS_Pr->SiS_VBInfo & SetCRT2ToLCD) {
       if(SiS_Pr->SiS_VBInfo & SetInSlaveMode) {
         SRdata |= 0x01;          			/* 8 dot clock  */
       }
     }
   }

   SRdata |= 0x20;                			/* screen off  */

   SiS_SetReg1(SiS_Pr->SiS_P3c4,0x01,SRdata);

   for(i = 2; i <= 4; i++) {
       	SRdata = SiS_Pr->SiS_StandTable[StandTableIndex].SR[i-1];
     	SiS_SetReg1(SiS_Pr->SiS_P3c4,i,SRdata);
   }
}

void
SiS_SetMiscRegs(SiS_Private *SiS_Pr, UCHAR *ROMAddr,USHORT StandTableIndex)
{
   UCHAR Miscdata;

   Miscdata = SiS_Pr->SiS_StandTable[StandTableIndex].MISC;

   if(SiS_Pr->SiS_VBType & VB_SIS301BLV302BLV) {
      if(SiS_Pr->SiS_VBInfo & SetCRT2ToLCDA) {
        Miscdata |= 0x0C;
      }
   }

   SiS_SetReg3(SiS_Pr->SiS_P3c2,Miscdata);
}

void
SiS_SetCRTCRegs(SiS_Private *SiS_Pr, UCHAR *ROMAddr,PSIS_HW_DEVICE_INFO HwDeviceExtension,
                USHORT StandTableIndex)
{
  UCHAR CRTCdata;
  USHORT i;

  SiS_SetRegAND(SiS_Pr->SiS_P3d4,0x11,0x7f);                       /* Unlock CRTC */

  for(i = 0; i <= 0x18; i++) {
     CRTCdata = SiS_Pr->SiS_StandTable[StandTableIndex].CRTC[i];
     SiS_SetReg1(SiS_Pr->SiS_P3d4,i,CRTCdata);                     /* Set CRTC(3d4) */
  }
  if( ( (HwDeviceExtension->jChipType == SIS_630) ||
        (HwDeviceExtension->jChipType == SIS_730) )  &&
      (HwDeviceExtension->jChipRevision >= 0x30) ) {       	   /* for 630S0 */
    if(SiS_Pr->SiS_VBInfo & SetInSlaveMode) {
      if(SiS_Pr->SiS_VBInfo & (SetCRT2ToLCD | SetCRT2ToTV)) {
         SiS_SetReg1(SiS_Pr->SiS_P3d4,0x18,0xFE);
      }
    }
  }
}

void
SiS_SetATTRegs(SiS_Private *SiS_Pr, UCHAR *ROMAddr,USHORT StandTableIndex,
               PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
   UCHAR ARdata;
   USHORT i;

   for(i = 0; i <= 0x13; i++) {
    ARdata = SiS_Pr->SiS_StandTable[StandTableIndex].ATTR[i];
#if 0
    if((i <= 0x0f) || (i == 0x11)) {
        if(ds:489 & 0x08) {
	   continue;
        }
    }
#endif
    if(i == 0x13) {
      /* Pixel shift. If screen on LCD or TV is shifted left or right, 
       * this might be the cause. 
       */
      if(SiS_Pr->SiS_VBType & VB_SIS301BLV302BLV) {
         if(SiS_Pr->SiS_VBInfo & SetCRT2ToLCDA)  ARdata=0;
      }
      if(SiS_Pr->SiS_IF_DEF_LVDS == 1) {
         if(SiS_Pr->SiS_IF_DEF_CH70xx != 0) {
            if(SiS_Pr->SiS_VBInfo & SetCRT2ToTV) {
               if(SiS_Pr->SiS_VBInfo & SetInSlaveMode) ARdata=0;
            }
         }
      }
      if(SiS_Pr->SiS_VBInfo & SetCRT2ToLCD) {
         if(HwDeviceExtension->jChipType >= SIS_315H) {
	    if(IS_SIS550650740660) {
	       /* 315, 330 don't do this */
	       if(SiS_Pr->SiS_VBType & VB_SIS301B302B) {
	          if(SiS_Pr->SiS_VBInfo & SetInSlaveMode) ARdata=0;
	       } else {
	          ARdata = 0;
	       }
	    }
	 } else {
           if(SiS_Pr->SiS_VBInfo & SetInSlaveMode)  ARdata=0;
	}
      }
    }
    SiS_GetReg2(SiS_Pr->SiS_P3da);                              /* reset 3da  */
    SiS_SetReg3(SiS_Pr->SiS_P3c0,i);                            /* set index  */
    SiS_SetReg3(SiS_Pr->SiS_P3c0,ARdata);                       /* set data   */
   }
   SiS_GetReg2(SiS_Pr->SiS_P3da);                               /* reset 3da  */
   SiS_SetReg3(SiS_Pr->SiS_P3c0,0x14);                          /* set index  */
   SiS_SetReg3(SiS_Pr->SiS_P3c0,0x00);                          /* set data   */

   SiS_GetReg2(SiS_Pr->SiS_P3da);
   SiS_SetReg3(SiS_Pr->SiS_P3c0,0x20);				/* Enable Attribute  */
   SiS_GetReg2(SiS_Pr->SiS_P3da);
}

void
SiS_SetGRCRegs(SiS_Private *SiS_Pr, UCHAR *ROMAddr,USHORT StandTableIndex)
{
   UCHAR GRdata;
   USHORT i;

   for(i = 0; i <= 0x08; i++) {
     GRdata = SiS_Pr->SiS_StandTable[StandTableIndex].GRC[i];
     SiS_SetReg1(SiS_Pr->SiS_P3ce,i,GRdata);                    /* Set GR(3ce) */
   }

   if(SiS_Pr->SiS_ModeType > ModeVGA) {
     SiS_SetRegAND(SiS_Pr->SiS_P3ce,0x05,0xBF);			/* 256 color disable */
   }
}

void
SiS_ClearExt1Regs(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
  USHORT i;

  for(i = 0x0A; i <= 0x0E; i++) {
      SiS_SetReg1(SiS_Pr->SiS_P3c4,i,0x00);      /* Clear SR0A-SR0E */
  }

  /* TW: 330, 650/LVDS/301LV, 740/LVDS */
  if(HwDeviceExtension->jChipType >= SIS_315H) {
     SiS_SetRegAND(SiS_Pr->SiS_P3c4,0x37,0xFE);
  }
}

void
SiS_SetSync(SiS_Private *SiS_Pr, UCHAR *ROMAddr,USHORT RefreshRateTableIndex)
{
  USHORT sync;
  USHORT temp;

  if(SiS_Pr->UseCustomMode) {
     sync = SiS_Pr->CInfoFlag >> 8;
  } else {
     sync = SiS_Pr->SiS_RefIndex[RefreshRateTableIndex].Ext_InfoFlag >> 8;
  }     

  sync &= 0xC0;
  temp = 0x2F | sync;
  SiS_SetReg3(SiS_Pr->SiS_P3c2,temp);           /* Set Misc(3c2) */
}

void
SiS_SetCRT1CRTC(SiS_Private *SiS_Pr, UCHAR *ROMAddr,USHORT ModeNo,USHORT ModeIdIndex,
                USHORT RefreshRateTableIndex,
		PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
  UCHAR  index;
  USHORT tempah,i,modeflag,j;
#ifdef SIS315H
  USHORT temp;
  USHORT ResIndex,DisplayType;
  const SiS_LCDACRT1DataStruct *LCDACRT1Ptr = NULL;
#endif

  SiS_SetRegAND(SiS_Pr->SiS_P3d4,0x11,0x7f);		/*unlock cr0-7  */

  if(SiS_Pr->UseCustomMode) {
     modeflag = SiS_Pr->CModeFlag;
  } else {
     if(ModeNo <= 0x13) {
        modeflag = SiS_Pr->SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;
     } else {
        modeflag = SiS_Pr->SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;
     }
  }     

  if((SiS_Pr->SiS_IF_DEF_LVDS == 0) && (SiS_Pr->SiS_VBInfo & SetCRT2ToLCDA)) {

#ifdef SIS315H

     /* LCDA */

     temp = SiS_GetLCDACRT1Ptr(SiS_Pr,ROMAddr,ModeNo,ModeIdIndex,
                       RefreshRateTableIndex,&ResIndex,&DisplayType);

     switch(DisplayType) {
      case Panel_800x600       : LCDACRT1Ptr = SiS_Pr->SiS_LCDACRT1800x600_1;      break;
      case Panel_1024x768      : LCDACRT1Ptr = SiS_Pr->SiS_LCDACRT11024x768_1;     break;
      case Panel_1280x1024     : LCDACRT1Ptr = SiS_Pr->SiS_LCDACRT11280x1024_1;    break;
      case Panel_1400x1050     : LCDACRT1Ptr = SiS_Pr->SiS_LCDACRT11400x1050_1;    break;
      case Panel_1600x1200     : LCDACRT1Ptr = SiS_Pr->SiS_LCDACRT11600x1200_1;    break;
      case Panel_800x600   + 16: LCDACRT1Ptr = SiS_Pr->SiS_LCDACRT1800x600_1_H;    break;
      case Panel_1024x768  + 16: LCDACRT1Ptr = SiS_Pr->SiS_LCDACRT11024x768_1_H;   break;
      case Panel_1280x1024 + 16: LCDACRT1Ptr = SiS_Pr->SiS_LCDACRT11280x1024_1_H;  break;
      case Panel_1400x1050 + 16: LCDACRT1Ptr = SiS_Pr->SiS_LCDACRT11400x1050_1_H;  break;
      case Panel_1600x1200 + 16: LCDACRT1Ptr = SiS_Pr->SiS_LCDACRT11600x1200_1_H;  break;
      case Panel_800x600   + 32: LCDACRT1Ptr = SiS_Pr->SiS_LCDACRT1800x600_2;      break;
      case Panel_1024x768  + 32: LCDACRT1Ptr = SiS_Pr->SiS_LCDACRT11024x768_2;     break;
      case Panel_1280x1024 + 32: LCDACRT1Ptr = SiS_Pr->SiS_LCDACRT11280x1024_2;    break;
      case Panel_1400x1050 + 32: LCDACRT1Ptr = SiS_Pr->SiS_LCDACRT11400x1050_2;    break;
      case Panel_1600x1200 + 32: LCDACRT1Ptr = SiS_Pr->SiS_LCDACRT11600x1200_2;    break;
      case Panel_800x600   + 48: LCDACRT1Ptr = SiS_Pr->SiS_LCDACRT1800x600_2_H;    break;
      case Panel_1024x768  + 48: LCDACRT1Ptr = SiS_Pr->SiS_LCDACRT11024x768_2_H;   break;
      case Panel_1280x1024 + 48: LCDACRT1Ptr = SiS_Pr->SiS_LCDACRT11280x1024_2_H;  break;
      case Panel_1400x1050 + 48: LCDACRT1Ptr = SiS_Pr->SiS_LCDACRT11400x1050_2_H;  break;
      case Panel_1600x1200 + 48: LCDACRT1Ptr = SiS_Pr->SiS_LCDACRT11600x1200_2_H;  break;
      default:                   LCDACRT1Ptr = SiS_Pr->SiS_LCDACRT11024x768_1;     break;
     }

     tempah = (LCDACRT1Ptr+ResIndex)->CR[0];
     SiS_SetReg1(SiS_Pr->SiS_P3d4,0x00,tempah);
     for(i=0x01,j=1;i<=0x07;i++,j++){
       tempah = (LCDACRT1Ptr+ResIndex)->CR[j];
       SiS_SetReg1(SiS_Pr->SiS_P3d4,i,tempah);
     }
     for(i=0x10,j=8;i<=0x12;i++,j++){
       tempah = (LCDACRT1Ptr+ResIndex)->CR[j];
       SiS_SetReg1(SiS_Pr->SiS_P3d4,i,tempah);
     }
     for(i=0x15,j=11;i<=0x16;i++,j++){
       tempah =(LCDACRT1Ptr+ResIndex)->CR[j];
       SiS_SetReg1(SiS_Pr->SiS_P3d4,i,tempah);
     }
     for(i=0x0A,j=13;i<=0x0C;i++,j++){
       tempah = (LCDACRT1Ptr+ResIndex)->CR[j];
       SiS_SetReg1(SiS_Pr->SiS_P3c4,i,tempah);
     }

     tempah = (LCDACRT1Ptr+ResIndex)->CR[16];
     tempah &= 0x0E0;
     SiS_SetReg1(SiS_Pr->SiS_P3c4,0x0E,tempah);

     tempah = (LCDACRT1Ptr+ResIndex)->CR[16];
     tempah &= 0x01;
     tempah <<= 5;
     if(modeflag & DoubleScanMode)  tempah |= 0x080;
     SiS_SetRegANDOR(SiS_Pr->SiS_P3d4,0x09,~0x020,tempah);

#endif

  } else {

     /* LVDS, 301, 301B, 301LV, 302LV, ... (non-LCDA) */

     if(SiS_Pr->UseCustomMode) {
     
        for(i=0,j=0;i<=07;i++,j++) {
          SiS_SetReg1(SiS_Pr->SiS_P3d4,j,SiS_Pr->CCRT1CRTC[i]);
        }
        for(j=0x10;i<=10;i++,j++) {
          SiS_SetReg1(SiS_Pr->SiS_P3d4,j,SiS_Pr->CCRT1CRTC[i]);
        }
        for(j=0x15;i<=12;i++,j++) {
          SiS_SetReg1(SiS_Pr->SiS_P3d4,j,SiS_Pr->CCRT1CRTC[i]);
        }
        for(j=0x0A;i<=15;i++,j++) {
          SiS_SetReg1(SiS_Pr->SiS_P3c4,j,SiS_Pr->CCRT1CRTC[i]);
        }

        tempah = SiS_Pr->CCRT1CRTC[16] & 0xE0;
        SiS_SetReg1(SiS_Pr->SiS_P3c4,0x0E,tempah);

        tempah = SiS_Pr->CCRT1CRTC[16];
        tempah &= 0x01;
        tempah <<= 5;
        if(modeflag & DoubleScanMode)  tempah |= 0x80;
        SiS_SetRegANDOR(SiS_Pr->SiS_P3d4,0x09,0xDF,tempah);
     
     
     } else {
     
        index = SiS_Pr->SiS_RefIndex[RefreshRateTableIndex].Ext_CRT1CRTC;  	/* Get index */
#if 0   /* Not any longer... */     
        if(HwDeviceExtension->jChipType < SIS_315H) {
           index &= 0x3F;
        }
#endif

        for(i=0,j=0;i<=07;i++,j++) {
          tempah=SiS_Pr->SiS_CRT1Table[index].CR[i];
          SiS_SetReg1(SiS_Pr->SiS_P3d4,j,tempah);
        }
        for(j=0x10;i<=10;i++,j++) {
          tempah=SiS_Pr->SiS_CRT1Table[index].CR[i];
          SiS_SetReg1(SiS_Pr->SiS_P3d4,j,tempah);
        }
        for(j=0x15;i<=12;i++,j++) {
          tempah=SiS_Pr->SiS_CRT1Table[index].CR[i];
          SiS_SetReg1(SiS_Pr->SiS_P3d4,j,tempah);
        }
        for(j=0x0A;i<=15;i++,j++) {
          tempah=SiS_Pr->SiS_CRT1Table[index].CR[i];
          SiS_SetReg1(SiS_Pr->SiS_P3c4,j,tempah);
        }

        tempah = SiS_Pr->SiS_CRT1Table[index].CR[16];
        tempah &= 0xE0;
        SiS_SetReg1(SiS_Pr->SiS_P3c4,0x0E,tempah);

        tempah = SiS_Pr->SiS_CRT1Table[index].CR[16];
        tempah &= 0x01;
        tempah <<= 5;
        if(modeflag & DoubleScanMode)  tempah |= 0x80;
        SiS_SetRegANDOR(SiS_Pr->SiS_P3d4,0x09,0xDF,tempah);

     }
  }

  if(SiS_Pr->SiS_ModeType > ModeVGA) SiS_SetReg1(SiS_Pr->SiS_P3d4,0x14,0x4F);
}

BOOLEAN
SiS_GetLCDACRT1Ptr(SiS_Private *SiS_Pr, UCHAR *ROMAddr,USHORT ModeNo,USHORT ModeIdIndex,
		   USHORT RefreshRateTableIndex,USHORT *ResIndex,
		   USHORT *DisplayType)
 {
  USHORT tempbx=0,modeflag=0;
  USHORT CRT2CRTC=0;

  if(ModeNo <= 0x13) {
  	modeflag = SiS_Pr->SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;
  	CRT2CRTC = SiS_Pr->SiS_SModeIDTable[ModeIdIndex].St_CRT2CRTC;
  } else {
  	modeflag = SiS_Pr->SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;
  	CRT2CRTC = SiS_Pr->SiS_RefIndex[RefreshRateTableIndex].Ext_CRT2CRTC;
  }

  tempbx = SiS_Pr->SiS_LCDResInfo;

  if(SiS_Pr->SiS_LCDInfo & DontExpandLCD) tempbx += 32;
  if(modeflag & HalfDCLK)                 tempbx += 16;

  *ResIndex = CRT2CRTC & 0x3F;
  *DisplayType = tempbx;

  return 1;
}

/* TW: Set offset and pitch - partly overruled by SetPitch() in XF86 */
void
SiS_SetCRT1Offset(SiS_Private *SiS_Pr, UCHAR *ROMAddr,USHORT ModeNo,USHORT ModeIdIndex,
                  USHORT RefreshRateTableIndex,
		  PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
   USHORT temp, DisplayUnit, infoflag;

   if(SiS_Pr->UseCustomMode) {
      infoflag = SiS_Pr->CInfoFlag;
   } else {
      infoflag = SiS_Pr->SiS_RefIndex[RefreshRateTableIndex].Ext_InfoFlag;
   }
   
   DisplayUnit = SiS_GetOffset(SiS_Pr,ROMAddr,ModeNo,ModeIdIndex,
                     RefreshRateTableIndex,HwDeviceExtension);		     

   temp = (DisplayUnit >> 8) & 0x0f;
   SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x0E,0xF0,temp);

   temp = DisplayUnit & 0xFF;
   SiS_SetReg1(SiS_Pr->SiS_P3d4,0x13,temp);

   if(infoflag & InterlaceMode) DisplayUnit >>= 1;

   DisplayUnit <<= 5;
   temp = (DisplayUnit & 0xff00) >> 8;
   if (DisplayUnit & 0xff) temp++;
   temp++;
   SiS_SetReg1(SiS_Pr->SiS_P3c4,0x10,temp);
}

/* TW: New from 650/LVDS 1.10.07, 630/301B and 630/LVDS BIOS */
void
SiS_ResetCRT1VCLK(SiS_Private *SiS_Pr, UCHAR *ROMAddr,PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
   USHORT index;

   /* TW: We only need to do this if Panel Link is to be
    *     initialized, thus on 630/LVDS/301BDH, and 650/LVDS
    */
   if(HwDeviceExtension->jChipType >= SIS_315H) {
      if(SiS_Pr->SiS_IF_DEF_LVDS == 0)  return;
   } else {
      if( (SiS_Pr->SiS_IF_DEF_LVDS == 0) &&
          (!(SiS_Pr->SiS_VBType & VB_SIS301BLV302BLV)) ) {
	 return;
      }
   }

   if(HwDeviceExtension->jChipType >= SIS_315H) {
   	SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x31,0xCF,0x20);
   } else {
   	SiS_SetReg1(SiS_Pr->SiS_P3c4,0x31,0x20);
   }
   index = 1;
   SiS_SetReg1(SiS_Pr->SiS_P3c4,0x2B,SiS_Pr->SiS_VCLKData[index].SR2B);
   SiS_SetReg1(SiS_Pr->SiS_P3c4,0x2C,SiS_Pr->SiS_VCLKData[index].SR2C);
   SiS_SetReg1(SiS_Pr->SiS_P3c4,0x2D,0x80);
   if(HwDeviceExtension->jChipType >= SIS_315H) {
   	SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x31,0xcf,0x10);
   } else {
   	SiS_SetReg1(SiS_Pr->SiS_P3c4,0x31,0x10);
   }
   index = 0;
   SiS_SetReg1(SiS_Pr->SiS_P3c4,0x2B,SiS_Pr->SiS_VCLKData[index].SR2B);
   SiS_SetReg1(SiS_Pr->SiS_P3c4,0x2C,SiS_Pr->SiS_VCLKData[index].SR2C);
   SiS_SetReg1(SiS_Pr->SiS_P3c4,0x2D,0x80);
}

void
SiS_SetCRT1VCLK(SiS_Private *SiS_Pr, UCHAR *ROMAddr,USHORT ModeNo,USHORT ModeIdIndex,
                PSIS_HW_DEVICE_INFO HwDeviceExtension,
		USHORT RefreshRateTableIndex)
{
  USHORT  index=0;

  if(!SiS_Pr->UseCustomMode) {
     index = SiS_GetVCLK2Ptr(SiS_Pr,ROMAddr,ModeNo,ModeIdIndex,
	                  RefreshRateTableIndex,HwDeviceExtension);
  }			  

  if( (SiS_Pr->SiS_VBType & VB_SIS301BLV302BLV) && (SiS_Pr->SiS_VBInfo & SetCRT2ToLCDA) ){

    	SiS_SetRegAND(SiS_Pr->SiS_P3c4,0x31,0xCF);

    	SiS_SetReg1(SiS_Pr->SiS_P3c4,0x2B,SiS_Pr->SiS_VBVCLKData[index].Part4_A);
    	SiS_SetReg1(SiS_Pr->SiS_P3c4,0x2C,SiS_Pr->SiS_VBVCLKData[index].Part4_B);

    	if(HwDeviceExtension->jChipType >= SIS_315H) {
		SiS_SetReg1(SiS_Pr->SiS_P3c4,0x2D,0x01);
   	} else {
    		SiS_SetReg1(SiS_Pr->SiS_P3c4,0x2D,0x80);
    	}

  } else {

	if(HwDeviceExtension->jChipType >= SIS_315H) {
	    SiS_SetRegAND(SiS_Pr->SiS_P3c4,0x31,0xCF);
	} else {
	    SiS_SetReg1(SiS_Pr->SiS_P3c4,0x31,0x00);
	}

	if(SiS_Pr->UseCustomMode) {
	   SiS_SetReg1(SiS_Pr->SiS_P3c4,0x2B,SiS_Pr->CSR2B);
	   SiS_SetReg1(SiS_Pr->SiS_P3c4,0x2C,SiS_Pr->CSR2C);
	} else {
    	   SiS_SetReg1(SiS_Pr->SiS_P3c4,0x2B,SiS_Pr->SiS_VCLKData[index].SR2B);
    	   SiS_SetReg1(SiS_Pr->SiS_P3c4,0x2C,SiS_Pr->SiS_VCLKData[index].SR2C);
  	}	   

    	if(HwDeviceExtension->jChipType >= SIS_315H) {
	    SiS_SetReg1(SiS_Pr->SiS_P3c4,0x2D,0x01);
	} else {
      	    SiS_SetReg1(SiS_Pr->SiS_P3c4,0x2D,0x80);
        }
  }
}

#if 0  /* TW: Not used */
void
SiS_IsLowResolution(SiS_Private *SiS_Pr, UCHAR *ROMAddr,USHORT ModeNo,USHORT ModeIdIndex)
{
  USHORT ModeFlag;

  SiS_SetRegAND(SiS_Pr->SiS_P3c4,0x0F,0x7F);

  if(ModeNo > 0x13) {
    ModeFlag = SiS_Pr->SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;
    if ((ModeFlag & HalfDCLK) && (ModeFlag & DoubleScanMode)) {
      SiS_SetRegOR(SiS_Pr->SiS_P3c4,0x0F,0x80);
      SiS_SetRegAND(SiS_Pr->SiS_P3c4,0x01,0xF7);
    }
  }
}
#endif

void
SiS_SetCRT1ModeRegs(SiS_Private *SiS_Pr, UCHAR *ROMAddr,PSIS_HW_DEVICE_INFO HwDeviceExtension,
                    USHORT ModeNo,USHORT ModeIdIndex,USHORT RefreshRateTableIndex)
{
  USHORT data,data2,data3;
  USHORT infoflag=0,modeflag;
  USHORT resindex,xres;
#ifdef SIS315H
  ULONG  longdata;
#endif  

  if(SiS_Pr->UseCustomMode) {
     modeflag = SiS_Pr->CModeFlag;
     infoflag = SiS_Pr->CInfoFlag;
  } else {
     if(ModeNo > 0x13) {
    	modeflag = SiS_Pr->SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;
    	infoflag = SiS_Pr->SiS_RefIndex[RefreshRateTableIndex].Ext_InfoFlag;
     } else {
    	modeflag = SiS_Pr->SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;
     }
  }

  SiS_SetRegAND(SiS_Pr->SiS_P3c4,0x1F,0x3F); 		/* DAC pedestal */

  if(ModeNo > 0x13) data = infoflag;
  else data = 0;

  data2 = 0;
  if(ModeNo > 0x13) {
     if(SiS_Pr->SiS_ModeType > 0x02) {
        data2 |= 0x02;
        data3 = (SiS_Pr->SiS_ModeType - ModeVGA) << 2;
        data2 |= data3;
     }
  }
#ifdef TWDEBUG
  xf86DrvMsg(0, X_INFO, "Debug: Mode infoflag = %x, Chiptype %d\n", 
  	data, HwDeviceExtension->jChipType);
#endif  
  if(data & InterlaceMode) data2 |= 0x20;
  SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x06,0xC0,data2);

  if(SiS_Pr->UseCustomMode) {
     xres = SiS_Pr->CHDisplay;
  } else {
     resindex = SiS_GetResInfo(SiS_Pr,ROMAddr,ModeNo,ModeIdIndex);
     if(ModeNo <= 0x13) {
      	xres = SiS_Pr->SiS_StResInfo[resindex].HTotal;
     } else {
      	xres = SiS_Pr->SiS_ModeResInfo[resindex].HTotal;
     }
  }

  if(HwDeviceExtension->jChipType != SIS_300) {
     data = 0x0000;
     if(infoflag & InterlaceMode) {
        if(xres <= 800)  data = 0x0020;
        else if(xres <= 1024) data = 0x0035;
        else data = 0x0048;
     }
     data2 = data & 0x00FF;
     SiS_SetReg1(SiS_Pr->SiS_P3d4,0x19,data2);
     data2 = (data & 0xFF00) >> 8;
     SiS_SetRegANDOR(SiS_Pr->SiS_P3d4,0x1a,0xFC,data2);
  }

  if(modeflag & HalfDCLK) {
     SiS_SetRegOR(SiS_Pr->SiS_P3c4,0x01,0x08);
  }

  if(HwDeviceExtension->jChipType == SIS_300) {
     if(modeflag & LineCompareOff) {
        SiS_SetRegOR(SiS_Pr->SiS_P3c4,0x0F,0x08);
     } else {
        SiS_SetRegAND(SiS_Pr->SiS_P3c4,0x0F,0xF7);
     }
  } else if(HwDeviceExtension->jChipType < SIS_315H) {
     if(modeflag & LineCompareOff) {
        SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x0F,0xB7,0x08);
     } else {
        SiS_SetRegAND(SiS_Pr->SiS_P3c4,0x0F,0xB7);
     }
     /* 630 BIOS does something for mode 0x12 here */
  } else {
     if(modeflag & LineCompareOff) {
        SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x0F,0xB7,0x08);
     } else {
        SiS_SetRegAND(SiS_Pr->SiS_P3c4,0x0F,0xB7);
     }
     /* 651 BIOS does something for mode 0x12 here */
  }

  if(HwDeviceExtension->jChipType != SIS_300) {
     if(SiS_Pr->SiS_ModeType == ModeEGA) {
        if(ModeNo > 0x13) {
  	   SiS_SetRegOR(SiS_Pr->SiS_P3c4,0x0F,0x40);
        }
     }
  }

#ifdef SIS315H
  /* TW: 315 BIOS sets SR17 at this point */
  if(HwDeviceExtension->jChipType == SIS_315PRO) {
      data = SiS_Get310DRAMType(SiS_Pr,ROMAddr,HwDeviceExtension);
      data = SiS_Pr->SiS_SR15[2][data];
      if(SiS_Pr->SiS_ModeType == ModeText) {
          data &= 0xc7;
      } else {
          data2 = SiS_GetOffset(SiS_Pr,ROMAddr,ModeNo,ModeIdIndex,
                                RefreshRateTableIndex,HwDeviceExtension);
	  data2 >>= 1;
	  if(infoflag & InterlaceMode) data2 >>= 1;
	  data3 = SiS_GetColorDepth(SiS_Pr,ROMAddr,ModeNo,ModeIdIndex);
	  data3 >>= 1;
	  if(data3 == 0) data3++;
	  data2 /= data3;
	  if(data2 >= 0x50) {
	      data &= 0x0f;
	      data |= 0x50;
	  }
      }
      SiS_SetReg1(SiS_Pr->SiS_P3c4,0x17,data);
  }

  /* TW: 330 BIOS sets SR17 at this point */
  if(HwDeviceExtension->jChipType == SIS_330) {
      data = SiS_Get310DRAMType(SiS_Pr,ROMAddr,HwDeviceExtension);
      data = SiS_Pr->SiS_SR15[2][data];
      if(SiS_Pr->SiS_ModeType <= ModeEGA) {
          data &= 0xc7;
      } else {
          if(SiS_Pr->UseCustomMode) {
	     data2 = SiS_Pr->CSRClock;
	  } else {
             data2 = SiS_GetVCLK2Ptr(SiS_Pr,ROMAddr,ModeNo,ModeIdIndex,
                               RefreshRateTableIndex,HwDeviceExtension);
             data2 = SiS_Pr->SiS_VCLKData[data2].CLOCK;
	  }

	  data3 = SiS_GetColorDepth(SiS_Pr,ROMAddr,ModeNo,ModeIdIndex);
	  data3 >>= 1;

	  data2 *= data3;

	  data3 = SiS_GetMCLK(SiS_Pr,ROMAddr, HwDeviceExtension);
	  longdata = data3 * 1024;

	  data2 = longdata / data2;

	  if(SiS_Pr->SiS_ModeType != Mode16Bpp) {
            if(data2 >= 0x19c)      data = 0xba;
	    else if(data2 >= 0x140) data = 0x7a;
	    else if(data2 >= 0x101) data = 0x3a;
	    else if(data2 >= 0xf5)  data = 0x32;
	    else if(data2 >= 0xe2)  data = 0x2a;
	    else if(data2 >= 0xc4)  data = 0x22;
	    else if(data2 >= 0xac)  data = 0x1a;
	    else if(data2 >= 0x9e)  data = 0x12;
	    else if(data2 >= 0x8e)  data = 0x0a;
	    else                    data = 0x02;
	  } else {
	    if(data2 >= 0x127)      data = 0xba;
	    else                    data = 0x7a;
	  }
      }
      SiS_SetReg1(SiS_Pr->SiS_P3c4,0x17,data);
  }
#endif

  data = 0x60;
  if(SiS_Pr->SiS_ModeType != ModeText) {
      data ^= 0x60;
      if(SiS_Pr->SiS_ModeType != ModeEGA) {
          data ^= 0xA0;
      }
  }
  SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x21,0x1F,data);

  SiS_SetVCLKState(SiS_Pr,ROMAddr,HwDeviceExtension,ModeNo,RefreshRateTableIndex,ModeIdIndex);

#ifdef SIS315H
  if(HwDeviceExtension->jChipType >= SIS_315H) {
    if(SiS_GetReg1(SiS_Pr->SiS_P3d4,0x31) & 0x40) {
        SiS_SetReg1(SiS_Pr->SiS_P3d4,0x52,0x2c);
    } else {
        SiS_SetReg1(SiS_Pr->SiS_P3d4,0x52,0x6c);
    }
  }
#endif
}

void
SiS_SetVCLKState(SiS_Private *SiS_Pr, UCHAR *ROMAddr,PSIS_HW_DEVICE_INFO HwDeviceExtension,
                 USHORT ModeNo,USHORT RefreshRateTableIndex,
                 USHORT ModeIdIndex)
{
  USHORT data, data2=0;
  USHORT VCLK, index=0;

  if (ModeNo <= 0x13) VCLK = 0;
  else {
     if(SiS_Pr->UseCustomMode) {
        VCLK = SiS_Pr->CSRClock;
     } else {
        index = SiS_GetVCLK2Ptr(SiS_Pr,ROMAddr,ModeNo,ModeIdIndex,
	               RefreshRateTableIndex,HwDeviceExtension);
        VCLK = SiS_Pr->SiS_VCLKData[index].CLOCK;
     }	
  }

  if(HwDeviceExtension->jChipType < SIS_315H) {		/* 300 series */

    data2 = 0x00;
    if(VCLK > 150) data2 |= 0x80;
    SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x07,0x7B,data2); 	/* DAC speed */

    data2 = 0x00;
    if(VCLK >= 150) data2 |= 0x08;       	/* VCLK > 150 */
    SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x32,0xF7,data2);

  } else { 						/* 315 series */

    data = 0;
    if(VCLK >= 166) data |= 0x0c;         	/* TW: Was 200; is 166 in 650, 315 and 330 BIOSes */
    SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x32,0xf3,data);

    if(VCLK >= 166) {				/* TW: Was 200, is 166 in 650, 315 and 330 BIOSes */
       SiS_SetRegAND(SiS_Pr->SiS_P3c4,0x1f,0xe7);
    }
  }

  data2 = 0x03;
  if((VCLK >= 135) && (VCLK < 160)) data2 = 0x02;
  if((VCLK >= 160) && (VCLK < 260)) data2 = 0x01;
  if(VCLK >= 260) data2 = 0x00;

  if(HwDeviceExtension->jChipType == SIS_540) {
    	if((VCLK == 203) || (VCLK < 234)) data2 = 0x02;
  }
  
  if(HwDeviceExtension->jChipType < SIS_315H) {
      SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x07,0xFC,data2);  	/* DAC speed */
  } else {
      if(HwDeviceExtension->jChipType > SIS_315PRO) {
         /* TW: This "if" is done in 330 and 650/LVDS/301LV BIOSes; Not in 315 BIOS */
         if(ModeNo > 0x13) data2 &= 0xfc;
      }
      SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x07,0xF8,data2);  	/* DAC speed */
  }
}

void
SiS_LoadDAC(SiS_Private *SiS_Pr,PSIS_HW_DEVICE_INFO HwDeviceExtension,
            UCHAR *ROMAddr,USHORT ModeNo,USHORT ModeIdIndex)
{
   USHORT data,data2;
   USHORT time,i,j,k;
   USHORT m,n,o;
   USHORT si,di,bx,dl;
   USHORT al,ah,dh;
   USHORT DACAddr, DACData, shiftflag;
   const USHORT *table = NULL;
#if 0
   USHORT tempah,tempch,tempcl,tempdh,tempal,tempbx;
#endif

   if(ModeNo <= 0x13) {
        data = SiS_Pr->SiS_SModeIDTable[ModeIdIndex].St_ModeFlag;
   } else {
        if(SiS_Pr->UseCustomMode) {
	   data = SiS_Pr->CModeFlag;
	} else {
           data = SiS_Pr->SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;
 	}	   
   }

#if 0
   if(!(ds:489 & 0x08)) {
#endif

	data &= DACInfoFlag;
	time = 64;
	if(data == 0x00) table = SiS_MDA_DAC;
	if(data == 0x08) table = SiS_CGA_DAC;
	if(data == 0x10) table = SiS_EGA_DAC;
	if(data == 0x18) {
	   time = 256;
	   table = SiS_VGA_DAC;
	}
	if(time == 256) j = 16;
	else            j = time;

	if( ( (SiS_Pr->SiS_VBInfo & SetCRT2ToLCD) &&        /* 301B-DH LCD */
	      (SiS_Pr->SiS_VBType & VB_NoLCD) )        ||
	    (SiS_Pr->SiS_VBInfo & SetCRT2ToLCDA)       ||   /* LCDA */
	    (!(SiS_Pr->SiS_SetFlag & ProgrammingCRT2)) ) {  /* Programming CRT1 */
	   DACAddr = SiS_Pr->SiS_P3c8;
	   DACData = SiS_Pr->SiS_P3c9;
	   shiftflag = 0;
	   SiS_SetReg3(SiS_Pr->SiS_P3c6,0xFF);
	} else {
	   shiftflag = 1;
	   DACAddr = SiS_Pr->SiS_Part5Port;
	   DACData = SiS_Pr->SiS_Part5Port + 1;
	}

	SiS_SetReg3(DACAddr,0x00);

	for(i=0; i<j; i++) {
	   data = table[i];
	   for(k=0; k<3; k++) {
		data2 = 0;
		if(data & 0x01) data2 = 0x2A;
		if(data & 0x02) data2 += 0x15;
		if(shiftflag) data2 <<= 2;
		SiS_SetReg3(DACData,data2);
		data >>= 2;
	   }
	}

	if(time == 256) {
	   for(i = 16; i < 32; i++) {
		data = table[i];
		if(shiftflag) data <<= 2;
		for(k=0; k<3; k++) SiS_SetReg3(DACData,data);
	   }
	   si = 32;
	   for(m = 0; m < 9; m++) {
	      di = si;
	      bx = si + 4;
	      dl = 0;
	      for(n = 0; n < 3; n++) {
		 for(o = 0; o < 5; o++) {
		    dh = table[si];
		    ah = table[di];
		    al = table[bx];
		    si++;
		    SiS_WriteDAC(SiS_Pr,DACData,shiftflag,dl,ah,al,dh);
		 }
		 si -= 2;
		 for(o = 0; o < 3; o++) {
		    dh = table[bx];
		    ah = table[di];
		    al = table[si];
		    si--;
		    SiS_WriteDAC(SiS_Pr,DACData,shiftflag,dl,ah,al,dh);
		 }
		 dl++;
	      }            /* for n < 3 */
	      si += 5;
	   }               /* for m < 9 */
	}
#if 0
    }  /* ds:489 & 0x08 */
#endif

#if 0
    if((!(ds:489 & 0x08)) && (ds:489 & 0x06)) {
           tempbx = 0;
	   for(i=0; i< 256; i++) {
               SiS_SetReg3(SiS_Pr->SiS_P3c8-1,tempbx);    	/* 7f87 */
               tempah = SiS_GetReg3(SiS_Pr->SiS_P3c8+1);  	/* 7f83 */
	       tempch = SiS_GetReg3(SiS_Pr->SiS_P3c8+1);
	       tempcl = SiS_GetReg3(SiS_Pr->SiS_P3c8+1);
	       tempdh = tempah;
	       tempal = 0x4d * tempdh;          	/* 7fb8 */
	       tempbx += tempal;
	       tempal = 0x97 * tempch;
	       tempbx += tempal;
	       tempal = 0x1c * tempcl;
	       tempbx += tempal;
	       if((tempbx & 0x00ff) > 0x80) tempbx += 0x100;
	       tempdh = (tempbx & 0x00ff) >> 8;
	       tempch = tempdh;
	       tempcl = tempdh;
	       SiS_SetReg3(SiS_Pr->SiS_P3c8,(tempbx & 0xff));  	/* 7f7c */
	       SiS_SetReg3(SiS_Pr->SiS_P3c8+1,tempdh);          /* 7f92 */
	       SiS_SetReg3(SiS_Pr->SiS_P3c8+1,tempch);
	       SiS_SetReg3(SiS_Pr->SiS_P3c8+1,tempcl);
           }
    }
#endif
}

void
SiS_WriteDAC(SiS_Private *SiS_Pr, USHORT DACData, USHORT shiftflag,
             USHORT dl, USHORT ah, USHORT al, USHORT dh)
{
  USHORT temp;
  USHORT bh,bl;

  bh = ah;
  bl = al;
  if(dl != 0) {
    temp = bh;
    bh = dh;
    dh = temp;
    if(dl == 1) {
       temp = bl;
       bl = dh;
       dh = temp;
    } else {
       temp = bl;
       bl = bh;
       bh = temp;
    }
  }
  if(shiftflag) {
     dh <<= 2;
     bh <<= 2;
     bl <<= 2;
  }
  SiS_SetReg3(DACData,(USHORT)dh);
  SiS_SetReg3(DACData,(USHORT)bh);
  SiS_SetReg3(DACData,(USHORT)bl);
}

#ifndef LINUX_XF86
static ULONG
GetDRAMSize(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
  ULONG   AdapterMemorySize = 0;
#ifdef SIS315H
  USHORT  counter;
#endif

  switch(HwDeviceExtension->jChipType) {
#ifdef SIS315H
  case SIS_315H:
  case SIS_315:
  case SIS_315PRO:
    	counter = SiS_GetReg1(SiS_Pr->SiS_P3c4,0x14);
	AdapterMemorySize = 1 << ((counter & 0xF0) >> 4);
	counter >>= 2;
	counter &= 0x03;
	if(counter == 0x02) {
		AdapterMemorySize += (AdapterMemorySize / 2);      /* DDR asymetric */
	} else if(counter != 0) {
		AdapterMemorySize <<= 1;                           /* SINGLE_CHANNEL_2_RANK or DUAL_CHANNEL_1_RANK */
	}
	AdapterMemorySize *= (1024*1024);
        break;

  case SIS_330:
    	counter = SiS_GetReg1(SiS_Pr->SiS_P3c4,0x14);
	AdapterMemorySize = 1 << ((counter & 0xF0) >> 4);
	counter &= 0x0c;
	if(counter != 0) {
		AdapterMemorySize <<= 1;
	}
	AdapterMemorySize *= (1024*1024);
	break;

  case SIS_550:
  case SIS_650:
  case SIS_740:
  case SIS_660:
  case SIS_760:
  	counter = SiS_GetReg1(SiS_Pr->SiS_P3c4,0x14) & 0x3F;
      	counter++;
      	AdapterMemorySize = counter * 4;
      	AdapterMemorySize *= (1024*1024);
	break;
#endif

#ifdef SIS300
  case SIS_300:
  case SIS_540:
  case SIS_630:
  case SIS_730:
      	AdapterMemorySize = SiS_GetReg1(SiS_Pr->SiS_P3c4,0x14) & 0x3F;
      	AdapterMemorySize++;
      	AdapterMemorySize *= (1024*1024);
	break;
#endif
  default:
        break;
  }

  return AdapterMemorySize;
}
#endif

#ifndef LINUX_XF86
void
SiS_ClearBuffer(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension,USHORT ModeNo)
{
  PVOID   VideoMemoryAddress = (PVOID)HwDeviceExtension->pjVideoMemoryAddress;
  ULONG   AdapterMemorySize  = (ULONG)HwDeviceExtension->ulVideoMemorySize;
  PUSHORT pBuffer;
  int i;

  if (SiS_Pr->SiS_ModeType>=ModeEGA) {
    if(ModeNo > 0x13) {
      AdapterMemorySize = GetDRAMSize(SiS_Pr, HwDeviceExtension);
      SiS_SetMemory(VideoMemoryAddress,AdapterMemorySize,0);
    } else {
      pBuffer = VideoMemoryAddress;
      for(i=0; i<0x4000; i++)
         pBuffer[i] = 0x0000;
    }
  } else {
    pBuffer = VideoMemoryAddress;
    if (SiS_Pr->SiS_ModeType < ModeCGA) {
      for(i=0; i<0x4000; i++)
         pBuffer[i] = 0x0720;
    } else {
      SiS_SetMemory(VideoMemoryAddress,0x8000,0);
    }
  }
}
#endif

void
SiS_DisplayOn(SiS_Private *SiS_Pr)
{
   SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x01,0xDF,0x00);
}

void
SiS_DisplayOff(SiS_Private *SiS_Pr)
{
   SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x01,0xDF,0x20);
}


/* ========================================== */
/*  SR CRTC GR */
void
SiS_SetReg1(USHORT port, USHORT index, USHORT data)
{
   OutPortByte(port,index);
   OutPortByte(port+1,data);
}

/* ========================================== */
/*  AR(3C0) */
void
SiS_SetReg2(SiS_Private *SiS_Pr, USHORT port, USHORT index, USHORT data)
{
   InPortByte(port+0x3da-0x3c0);
   OutPortByte(SiS_Pr->SiS_P3c0,index);
   OutPortByte(SiS_Pr->SiS_P3c0,data);
   OutPortByte(SiS_Pr->SiS_P3c0,0x20);
}

void
SiS_SetReg3(USHORT port, USHORT data)
{
   OutPortByte(port,data);
}

void
SiS_SetReg4(USHORT port, ULONG data)
{
   OutPortLong(port,data);
}

void
SiS_SetReg5(USHORT port, USHORT data)
{
   OutPortWord(port,data);
}

UCHAR SiS_GetReg1(USHORT port, USHORT index)
{
   UCHAR   data;

   OutPortByte(port,index);
   data = InPortByte(port+1);

   return(data);
}

UCHAR
SiS_GetReg2(USHORT port)
{
   UCHAR   data;

   data= InPortByte(port);

   return(data);
}

ULONG
SiS_GetReg3(USHORT port)
{
   ULONG   data;

   data = InPortLong(port);

   return(data);
}

USHORT
SiS_GetReg4(USHORT port)
{
   ULONG   data;

   data = InPortWord(port);

   return(data);
}

void
SiS_ClearDAC(SiS_Private *SiS_Pr, ULONG port)
{
   int i;

   OutPortByte(port, 0);
   port++;
   for (i=0; i < (256 * 3); i++) {
      OutPortByte(port, 0);
   }

}

#if 0  /* TW: Unused */
void
SiS_SetInterlace(SiS_Private *SiS_Pr, UCHAR *ROMAddr,USHORT ModeNo,USHORT RefreshRateTableIndex)
{
  ULONG Temp;
  USHORT data,Temp2;

  if (ModeNo<=0x13) return;

  Temp = (ULONG)SiS_GetReg1(SiS_Pr->SiS_P3d4,0x01);
  Temp++;
  Temp <<= 3;

  if(Temp == 1024) data = 0x0035;
  else if(Temp == 1280) data = 0x0048;
  else data = 0x0000;

  Temp2 = SiS_Pr->SiS_RefIndex[RefreshRateTableIndex].Ext_InfoFlag;
  Temp2 &= InterlaceMode;
  if(Temp2 == 0) data=0x0000;

  SiS_SetReg1(SiS_Pr->SiS_P3d4,0x19,data);

  Temp = (ULONG)SiS_GetReg1(SiS_Pr->SiS_P3d4,0x1A);
  Temp = (USHORT)(Temp & 0xFC);
  SiS_SetReg1(SiS_Pr->SiS_P3d4,0x1A,(USHORT)Temp);

  Temp = (ULONG)SiS_GetReg1(SiS_Pr->SiS_P3c4,0x0f);
  Temp2 = (USHORT)Temp & 0xBF;
  if(ModeNo==0x37) Temp2 |= 0x40;
  SiS_SetReg1(SiS_Pr->SiS_P3d4,0x1A,(USHORT)Temp2);
}
#endif

#ifdef SIS315H
void
SiS_SetCRT1FIFO_310(SiS_Private *SiS_Pr, UCHAR *ROMAddr,USHORT ModeNo,USHORT ModeIdIndex,
                PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
  USHORT modeflag;

  SiS_SetRegAND(SiS_Pr->SiS_P3c4,0x3D,0xFE);  /* disable auto-threshold */

  if(ModeNo > 0x13) {
    if(SiS_Pr->UseCustomMode) {
       modeflag = SiS_Pr->CModeFlag;
    } else {
       modeflag = SiS_Pr->SiS_EModeIDTable[ModeIdIndex].Ext_ModeFlag;
    }       
    if( (!(modeflag & DoubleScanMode)) || (!(modeflag & HalfDCLK))) {
       SiS_SetReg1(SiS_Pr->SiS_P3c4,0x08,0x34);
       SiS_SetRegAND(SiS_Pr->SiS_P3c4,0x09,0xF0);
       SiS_SetRegOR(SiS_Pr->SiS_P3c4,0x3D,0x01);
    } else {
       SiS_SetReg1(SiS_Pr->SiS_P3c4,0x08,0xAE);
       SiS_SetRegAND(SiS_Pr->SiS_P3c4,0x09,0xF0);
    }
  } else {
    SiS_SetReg1(SiS_Pr->SiS_P3c4,0x08,0xAE);
    SiS_SetRegAND(SiS_Pr->SiS_P3c4,0x09,0xF0);
  }
}
#endif

#ifdef SIS300
void
SiS_SetCRT1FIFO_300(SiS_Private *SiS_Pr, UCHAR *ROMAddr,USHORT ModeNo,PSIS_HW_DEVICE_INFO HwDeviceExtension,
                    USHORT RefreshRateTableIndex)
{
  USHORT  ThresholdLow = 0;
  USHORT  index, VCLK, MCLK, colorth=0;
  USHORT  tempah, temp;

  if(ModeNo > 0x13) {

     if(SiS_Pr->UseCustomMode) {
        VCLK = SiS_Pr->CSRClock;
     } else {
        index = SiS_Pr->SiS_RefIndex[RefreshRateTableIndex].Ext_CRTVCLK;
        index &= 0x3F;
        VCLK = SiS_Pr->SiS_VCLKData[index].CLOCK;             /* Get VCLK  */
     }

     switch (SiS_Pr->SiS_ModeType - ModeEGA) {     /* Get half colordepth */
        case 0 : colorth = 1; break;
        case 1 : colorth = 1; break;
        case 2 : colorth = 2; break;
        case 3 : colorth = 2; break;
        case 4 : colorth = 3; break;
        case 5 : colorth = 4; break;
     }

     index = SiS_GetReg1(SiS_Pr->SiS_P3c4,0x3A);
     index &= 0x07;
     MCLK = SiS_Pr->SiS_MCLKData_0[index].CLOCK;           /* Get MCLK  */

     tempah = SiS_GetReg1(SiS_Pr->SiS_P3d4,0x35);
     tempah &= 0xc3;
     SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x16,0x3c,tempah);

     do {
        ThresholdLow = SiS_CalcDelay(SiS_Pr, ROMAddr, VCLK, colorth, MCLK);
        ThresholdLow++;
        if(ThresholdLow < 0x13) break;
        SiS_SetRegAND(SiS_Pr->SiS_P3c4,0x16,0xfc);
        ThresholdLow = 0x13;
        tempah = SiS_GetReg1(SiS_Pr->SiS_P3c4,0x16);
        tempah >>= 6;
        if(!(tempah)) break;
        tempah--;
        tempah <<= 6;
        SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x16,0x3f,tempah);
     } while(0);

  } else ThresholdLow = 2;

  /* Write CRT/CPU threshold low, CRT/Engine threshold high */
  temp = (ThresholdLow << 4) | 0x0f;
  SiS_SetReg1(SiS_Pr->SiS_P3c4,0x08,temp);

  temp = (ThresholdLow & 0x10) << 1;
  if(ModeNo > 0x13) temp |= 0x40;
  SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x0f,0x9f,temp);

  /* What is this? */
  SiS_SetReg1(SiS_Pr->SiS_P3c4,0x3B,0x09);

  /* Write CRT/CPU threshold high */
  temp = ThresholdLow + 3;
  if(temp > 0x0f) temp = 0x0f;
  SiS_SetReg1(SiS_Pr->SiS_P3c4,0x09,temp);
}

USHORT
SiS_CalcDelay(SiS_Private *SiS_Pr, UCHAR *ROMAddr, USHORT VCLK, USHORT colordepth, USHORT MCLK)
{
  USHORT tempax, tempbx;

  tempbx = SiS_DoCalcDelay(SiS_Pr, MCLK, VCLK, colordepth, 0);
  tempax = SiS_DoCalcDelay(SiS_Pr, MCLK, VCLK, colordepth, 1);
  if(tempax < 4) tempax = 4;
  tempax -= 4;
  if(tempbx < tempax) tempbx = tempax;
  return(tempbx);
}

USHORT
SiS_DoCalcDelay(SiS_Private *SiS_Pr, USHORT MCLK, USHORT VCLK, USHORT colordepth, USHORT key)
{
  const UCHAR ThLowA[]   = { 61, 3,52, 5,68, 7,100,11,
                             43, 3,42, 5,54, 7, 78,11,
                             34, 3,37, 5,47, 7, 67,11 };

  const UCHAR ThLowB[]   = { 81, 4,72, 6,88, 8,120,12,
                             55, 4,54, 6,66, 8, 90,12,
                             42, 4,45, 6,55, 8, 75,12 };

  const UCHAR ThTiming[] = {  1, 2, 2, 3, 0, 1,  1, 2 };

  USHORT tempah, tempal, tempcl, tempbx, temp;
  ULONG  longtemp;

  tempah = SiS_GetReg1(SiS_Pr->SiS_P3c4,0x18);
  tempah &= 0x62;
  tempah >>= 1;
  tempal = tempah;
  tempah >>= 3;
  tempal |= tempah;
  tempal &= 0x07;
  tempcl = ThTiming[tempal];
  tempbx = SiS_GetReg1(SiS_Pr->SiS_P3c4,0x16);
  tempbx >>= 6;
  tempah = SiS_GetReg1(SiS_Pr->SiS_P3c4,0x14);
  tempah >>= 4;
  tempah &= 0x0c;
  tempbx |= tempah;
  tempbx <<= 1;
  if(key == 0) {
     tempal = ThLowA[tempbx + 1];
     tempal *= tempcl;
     tempal += ThLowA[tempbx];
  } else {
     tempal = ThLowB[tempbx + 1];
     tempal *= tempcl;
     tempal += ThLowB[tempbx];
  }
  longtemp = tempal * VCLK * colordepth;
  temp = longtemp % (MCLK * 16);
  longtemp /= (MCLK * 16);
  if(temp) longtemp++;
  return((USHORT)longtemp);
}

void
SiS_SetCRT1FIFO_630(SiS_Private *SiS_Pr, UCHAR *ROMAddr,USHORT ModeNo,
 		    PSIS_HW_DEVICE_INFO HwDeviceExtension,
                    USHORT RefreshRateTableIndex)
{
  USHORT  i,index,data,VCLK,MCLK,colorth=0;
  ULONG   B,eax,bl,data2;
  USHORT  ThresholdLow=0;
  UCHAR   FQBQData[]= { 
  	0x01,0x21,0x41,0x61,0x81,
        0x31,0x51,0x71,0x91,0xb1,
        0x00,0x20,0x40,0x60,0x80,
        0x30,0x50,0x70,0x90,0xb0,
	0xFF
  };
  UCHAR   FQBQData730[]= {
        0x34,0x74,0xb4,
	0x23,0x63,0xa3,
	0x12,0x52,0x92,
	0x01,0x41,0x81,
	0x00,0x40,0x80,
	0xff
  };

  i=0;
  if(ModeNo > 0x13) {
    if(SiS_Pr->UseCustomMode) {
       VCLK = SiS_Pr->CSRClock;
    } else {
       index = SiS_Pr->SiS_RefIndex[RefreshRateTableIndex].Ext_CRTVCLK;
       index &= 0x3F;
       VCLK = SiS_Pr->SiS_VCLKData[index].CLOCK;          /* Get VCLK  */
    }       

    index = SiS_GetReg1(SiS_Pr->SiS_P3c4,0x1A);
    index &= 0x07;
    MCLK = SiS_Pr->SiS_MCLKData_0[index].CLOCK;           /* Get MCLK  */

    data2 = SiS_Pr->SiS_ModeType - ModeEGA;	  /* Get half colordepth */
    switch (data2) {
        case 0 : colorth = 1; break;
        case 1 : colorth = 1; break;
        case 2 : colorth = 2; break;
        case 3 : colorth = 2; break;
        case 4 : colorth = 3; break;
        case 5 : colorth = 4; break;
    }

    if(HwDeviceExtension->jChipType == SIS_730) {
    
       do {
          B = SiS_CalcDelay2(SiS_Pr, ROMAddr, FQBQData730[i], HwDeviceExtension) * VCLK * colorth;
	  bl = B / (MCLK * 16);

          if(B == bl * 16 * MCLK) {
             bl = bl + 1;
          } else {
             bl = bl + 2;
          }

          if(bl > 0x13) {
             if(FQBQData730[i+1] == 0xFF) {
                ThresholdLow = 0x13;
                break;
             }
             i++;
          } else {
             ThresholdLow = bl;
             break;
          }
       } while(FQBQData730[i] != 0xFF);
       
    } else {
    
       do {
          B = SiS_CalcDelay2(SiS_Pr, ROMAddr, FQBQData[i], HwDeviceExtension) * VCLK * colorth;
          bl = B / (MCLK * 16);

          if(B == bl * 16 * MCLK) {
             bl = bl + 1;
          } else {
             bl = bl + 2;
          }

          if(bl > 0x13) {
             if(FQBQData[i+1] == 0xFF) {
                ThresholdLow = 0x13;
                break;
             }
             i++;
          } else {
             ThresholdLow = bl;
             break;
          }
       } while(FQBQData[i] != 0xFF);
    }
  }
  else {
    if(HwDeviceExtension->jChipType == SIS_730) { 
    } else {
      i = 9;
    }
    ThresholdLow = 0x02;
  }

  /* Write foreground and background queue */
  if(HwDeviceExtension->jChipType == SIS_730) {  
   
     data2 = FQBQData730[i];
     data2 = (data2 & 0xC0) >> 5;
     data2 <<= 8;

#ifndef LINUX_XF86
     SiS_SetReg4(0xcf8,0x80000050);
     eax = SiS_GetReg3(0xcfc);
     eax &= 0xfffff9ff;
     eax |= data2;
     SiS_SetReg4(0xcfc,eax);
#else
     /* We use pci functions X offers. We use pcitag 0, because
      * we want to read/write to the host bridge (which is always
      * 00:00.0 on 630, 730 and 540), not the VGA device.
      */
     eax = pciReadLong(0x00000000, 0x50);
     eax &= 0xfffff9ff;
     eax |= data2;
     pciWriteLong(0x00000000, 0x50, eax);
#endif

     /* Write GUI grant timer (PCI config 0xA3) */
     data2 = FQBQData730[i] << 8;
     data2 = (data2 & 0x0f00) | ((data2 & 0x3000) >> 8);
     data2 <<= 20;
     
#ifndef LINUX_XF86
     SiS_SetReg4(0xcf8,0x800000A0);
     eax = SiS_GetReg3(0xcfc);
     eax &= 0x00ffffff;
     eax |= data2;
     SiS_SetReg4(0xcfc,eax);
#else
     eax = pciReadLong(0x00000000, 0xA0);
     eax &= 0x00ffffff;
     eax |= data2;
     pciWriteLong(0x00000000, 0xA0, eax);
#endif          

  } else {
  
     data2 = FQBQData[i];
     data2 = (data2 & 0xf0) >> 4;
     data2 <<= 24;

#ifndef LINUX_XF86
     SiS_SetReg4(0xcf8,0x80000050);
     eax = SiS_GetReg3(0xcfc);
     eax &= 0xf0ffffff;
     eax |= data2;
     SiS_SetReg4(0xcfc,eax);
#else
     eax = pciReadLong(0x00000000, 0x50);
     eax &= 0xf0ffffff;
     eax |= data2;
     pciWriteLong(0x00000000, 0x50, eax);
#endif

     /* Write GUI grant timer (PCI config 0xA3) */
     data2 = FQBQData[i];
     data2 &= 0x0f;
     data2 <<= 24;

#ifndef LINUX_XF86
     SiS_SetReg4(0xcf8,0x800000A0);
     eax = SiS_GetReg3(0xcfc);
     eax &= 0xf0ffffff;
     eax |= data2;
     SiS_SetReg4(0xcfc,eax);
#else
     eax = pciReadLong(0x00000000, 0xA0);
     eax &= 0xf0ffffff;
     eax |= data2;
     pciWriteLong(0x00000000, 0xA0, eax);
#endif
     
  }

  /* Write CRT/CPU threshold low, CRT/Engine threshold high */
  data = ((ThresholdLow & 0x0f) << 4) | 0x0f;
  SiS_SetReg1(SiS_Pr->SiS_P3c4,0x08,data);

  data = (ThresholdLow & 0x10) << 1;
  SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x0F,0xDF,data);

  /* What is this? */
  SiS_SetReg1(SiS_Pr->SiS_P3c4,0x3B,0x09);

  /* Write CRT/CPU threshold high (gap = 3) */
  data = ThresholdLow + 3;
  if(data > 0x0f) data = 0x0f;
  SiS_SetRegANDOR(SiS_Pr->SiS_P3c4,0x09,0x80,data);
}

USHORT
SiS_CalcDelay2(SiS_Private *SiS_Pr, UCHAR *ROMAddr,UCHAR key, PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
  USHORT data,index;
  const UCHAR  LatencyFactor[] = { 
   	97, 88, 86, 79, 77, 00,       /*; 64  bit    BQ=2   */
        00, 87, 85, 78, 76, 54,       /*; 64  bit    BQ=1   */
        97, 88, 86, 79, 77, 00,       /*; 128 bit    BQ=2   */
        00, 79, 77, 70, 68, 48,       /*; 128 bit    BQ=1   */
        80, 72, 69, 63, 61, 00,       /*; 64  bit    BQ=2   */
        00, 70, 68, 61, 59, 37,       /*; 64  bit    BQ=1   */
        86, 77, 75, 68, 66, 00,       /*; 128 bit    BQ=2   */
        00, 68, 66, 59, 57, 37        /*; 128 bit    BQ=1   */
  };
  const UCHAR  LatencyFactor730[] = {
         69, 63, 61, 
	 86, 79, 77,
	103, 96, 94,
	120,113,111,
	137,130,128,    /* --- Table ends with this entry, data below */
	137,130,128,	/* to avoid using illegal values              */
	137,130,128,
	137,130,128,
	137,130,128,
	137,130,128,
	137,130,128,
	137,130,128,
	137,130,128,
	137,130,128,
	137,130,128,
	137,130,128,
  };

  if(HwDeviceExtension->jChipType == SIS_730) {
     index = ((key & 0x0f) * 3) + ((key & 0xC0) >> 6);
     data = LatencyFactor730[index];
  } else {			    
     index = (key & 0xE0) >> 5;
     if(key & 0x10) index +=6;
     if(!(key & 0x01)) index += 24;
     data = SiS_GetReg1(SiS_Pr->SiS_P3c4,0x14);
     if(data & 0x0080) index += 12;
     data = LatencyFactor[index];
  }
  return(data);
}
#endif

#ifdef LINUX_XF86
BOOLEAN
SiS_GetPanelID(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension)
{
  const USHORT PanelTypeTable300[16] = {
      0xc101, 0xc117, 0x0121, 0xc135, 0xc142, 0xc152, 0xc162, 0xc072,
      0xc181, 0xc192, 0xc1a1, 0xc1b6, 0xc1c2, 0xc0d2, 0xc1e2, 0xc1f2
  };
  const USHORT PanelTypeTable31030x[16] = {
      0xc102, 0xc112, 0x0122, 0xc132, 0xc142, 0xc152, 0xc169, 0xc179,
      0x0189, 0xc192, 0xc1a2, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
  };
  const USHORT PanelTypeTable310LVDS[16] = {
      0xc111, 0xc122, 0xc133, 0xc144, 0xc155, 0xc166, 0xc177, 0xc188,
      0xc199, 0xc0aa, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
  };
  USHORT tempax,tempbx,tempah,temp;

  if(HwDeviceExtension->jChipType < SIS_315H) {

    tempax = SiS_GetReg1(SiS_Pr->SiS_P3c4,0x18);
    tempbx = tempax & 0x0F;
    if(!(tempax & 0x10)){
      if(SiS_Pr->SiS_IF_DEF_LVDS == 1){
        tempbx = 0;
        temp = SiS_GetReg1(SiS_Pr->SiS_P3c4,0x38);
        if(temp & 0x40) tempbx |= 0x08;
        if(temp & 0x20) tempbx |= 0x02;
        if(temp & 0x01) tempbx |= 0x01;
        temp = SiS_GetReg1(SiS_Pr->SiS_P3c4,0x39);
        if(temp & 0x80) tempbx |= 0x04;
      } else {
        return 0;
      }
    }
    tempbx = PanelTypeTable300[tempbx];
    tempbx |= LCDSync;
    temp = tempbx & 0x00FF;
    SiS_SetReg1(SiS_Pr->SiS_P3d4,0x36,temp);
    temp = (tempbx & 0xFF00) >> 8;
    SiS_SetRegANDOR(SiS_Pr->SiS_P3d4,0x37,~(LCDSyncBit|LCDRGB18Bit),temp);

  } else {

    tempax = tempah = SiS_GetReg1(SiS_Pr->SiS_P3c4,0x1a);
    tempax &= 0x1e;
    tempax >>= 1;
    if(SiS_Pr->SiS_IF_DEF_LVDS == 1) {
       if(tempax == 0) {
           /* TODO: Include HUGE detection routine
	            (Probably not worth bothering)
	    */
           return 0;
       }
       temp = tempax & 0xff;
       tempax--;
       tempbx = PanelTypeTable310LVDS[tempax];
    } else {
       tempbx = PanelTypeTable31030x[tempax];
       temp = tempbx & 0xff;
    }
    SiS_SetReg1(SiS_Pr->SiS_P3d4,0x36,temp);
    tempbx = (tempbx & 0xff00) >> 8;
    temp = tempbx & 0xc1;
    SiS_SetRegANDOR(SiS_Pr->SiS_P3d4,0x37,~(LCDSyncBit|LCDRGB18Bit),temp);
    if(SiS_Pr->SiS_IF_DEF_LVDS == 0) {
       temp = tempbx & 0x04;
       SiS_SetRegANDOR(SiS_Pr->SiS_P3d4,0x39,0xfb,temp);
    }

  }
  return 1;
}
#endif

/* ================ XFREE86 ================= */

/* Helper functions */

#ifdef LINUX_XF86
USHORT
SiS_CheckBuildCustomMode(ScrnInfoPtr pScrn, DisplayModePtr mode, int VBFlags)
{
   SISPtr pSiS = SISPTR(pScrn);
   int    out_n, out_dn, out_div, out_sbit, out_scale;
   int    depth = pSiS->CurrentLayout.bitsPerPixel;
   unsigned int vclk[5];

#define Midx         0
#define Nidx         1
#define VLDidx       2
#define Pidx         3
#define PSNidx       4

   pSiS->SiS_Pr->CModeFlag = 0;
   
   pSiS->SiS_Pr->CDClock = mode->Clock;

   pSiS->SiS_Pr->CHDisplay = mode->HDisplay;
   pSiS->SiS_Pr->CHSyncStart = mode->HSyncStart;
   pSiS->SiS_Pr->CHSyncEnd = mode->HSyncEnd;
   pSiS->SiS_Pr->CHTotal = mode->HTotal;

   pSiS->SiS_Pr->CVDisplay = mode->VDisplay;
   pSiS->SiS_Pr->CVSyncStart = mode->VSyncStart;
   pSiS->SiS_Pr->CVSyncEnd = mode->VSyncEnd;
   pSiS->SiS_Pr->CVTotal = mode->VTotal;

   pSiS->SiS_Pr->CFlags = mode->Flags;

   if(pSiS->SiS_Pr->CFlags & V_INTERLACE) {
         pSiS->SiS_Pr->CVDisplay >>= 1;
	 pSiS->SiS_Pr->CVSyncStart >>= 1;
	 pSiS->SiS_Pr->CVSyncEnd >>= 1;
	 pSiS->SiS_Pr->CVTotal >>= 1;
   }
   if(pSiS->SiS_Pr->CFlags & V_DBLSCAN) {
         /* pSiS->SiS_Pr->CDClock <<= 1; */
	 pSiS->SiS_Pr->CVDisplay <<= 1;
	 pSiS->SiS_Pr->CVSyncStart <<= 1;
	 pSiS->SiS_Pr->CVSyncEnd <<= 1;
	 pSiS->SiS_Pr->CVTotal <<= 1;
   }

   pSiS->SiS_Pr->CHBlankStart = pSiS->SiS_Pr->CHDisplay;
   pSiS->SiS_Pr->CHBlankEnd = pSiS->SiS_Pr->CHTotal;
   pSiS->SiS_Pr->CVBlankStart = pSiS->SiS_Pr->CVSyncStart - 1;
   pSiS->SiS_Pr->CVBlankEnd = pSiS->SiS_Pr->CVTotal;

   if(SiS_compute_vclk(pSiS->SiS_Pr->CDClock, &out_n, &out_dn, &out_div, &out_sbit, &out_scale)) {
      pSiS->SiS_Pr->CSR2B = (out_div == 2) ? 0x80 : 0x00;
      pSiS->SiS_Pr->CSR2B |= ((out_n - 1) & 0x7f);
      pSiS->SiS_Pr->CSR2C = (out_dn - 1) & 0x1f;
      pSiS->SiS_Pr->CSR2C |= (((out_scale - 1) & 3) << 5);
      pSiS->SiS_Pr->CSR2C |= ((out_sbit & 0x01) << 7);
#ifdef TWDEBUG
      xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Clock %d: n %d dn %d div %d sb %d sc %d\n",
        	pSiS->SiS_Pr->CDClock, out_n, out_dn, out_div, out_sbit, out_scale);
#endif
   } else {
      SiSCalcClock(pScrn, pSiS->SiS_Pr->CDClock, 2, vclk);
      pSiS->SiS_Pr->CSR2B = (vclk[VLDidx] == 2) ? 0x80 : 0x00;
      pSiS->SiS_Pr->CSR2B |= (vclk[Midx] - 1) & 0x7f;
      pSiS->SiS_Pr->CSR2C = (vclk[Nidx] - 1) & 0x1f;
      if(vclk[Pidx] <= 4) {
         /* postscale 1,2,3,4 */
         pSiS->SiS_Pr->CSR2C |= ((vclk[Pidx] - 1) & 3) << 5;
      } else {
         /* postscale 6,8 */
         pSiS->SiS_Pr->CSR2C |= (((vclk[Pidx] / 2) - 1) & 3) << 5;
	 pSiS->SiS_Pr->CSR2C |= 0x80;
      }
#ifdef TWDEBUG
      xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Clock %d: n %d dn %d div %d sc %d\n",
        	pSiS->SiS_Pr->CDClock, vclk[Midx], vclk[Nidx], vclk[VLDidx], vclk[Pidx]);
#endif
   }

   pSiS->SiS_Pr->CSRClock = (pSiS->SiS_Pr->CDClock / 1000) + 1;

   pSiS->SiS_Pr->CCRT1CRTC[0]  =  ((pSiS->SiS_Pr->CHTotal >> 3) - 5) & 0xff;
   pSiS->SiS_Pr->CCRT1CRTC[1]  =  (pSiS->SiS_Pr->CHDisplay >> 3) - 1;
   pSiS->SiS_Pr->CCRT1CRTC[2]  =  (pSiS->SiS_Pr->CHBlankStart >> 3) - 1;
   pSiS->SiS_Pr->CCRT1CRTC[3]  =  (((pSiS->SiS_Pr->CHBlankEnd >> 3) - 1) & 0x1F) | 0x80;
   pSiS->SiS_Pr->CCRT1CRTC[4]  =  (pSiS->SiS_Pr->CHSyncStart >> 3) + 3;
   pSiS->SiS_Pr->CCRT1CRTC[5]  =  ((((pSiS->SiS_Pr->CHBlankEnd >> 3) - 1) & 0x20) << 2) |
       				  (((pSiS->SiS_Pr->CHSyncEnd >> 3) + 3) & 0x1F);

   pSiS->SiS_Pr->CCRT1CRTC[6]  =  (pSiS->SiS_Pr->CVTotal - 2) & 0xFF;
   pSiS->SiS_Pr->CCRT1CRTC[7]  =  (((pSiS->SiS_Pr->CVTotal - 2) & 0x100) >> 8)
 	 			| (((pSiS->SiS_Pr->CVDisplay - 1) & 0x100) >> 7)
	 			| ((pSiS->SiS_Pr->CVSyncStart & 0x100) >> 6)
	 			| (((pSiS->SiS_Pr->CVBlankStart - 1) & 0x100) >> 5)
	 			| 0x10
	 			| (((pSiS->SiS_Pr->CVTotal - 2) & 0x200)   >> 4)
	 			| (((pSiS->SiS_Pr->CVDisplay - 1) & 0x200) >> 3)
	 			| ((pSiS->SiS_Pr->CVSyncStart & 0x200) >> 2);

   pSiS->SiS_Pr->CCRT1CRTC[16] = ((((pSiS->SiS_Pr->CVBlankStart - 1) & 0x200) >> 4) >> 5); 	/* cr9 */

#if 0
   if (mode->VScan >= 32)
	regp->CRTC[9] |= 0x1F;
   else if (mode->VScan > 1)
	regp->CRTC[9] |= mode->VScan - 1;
#endif

   pSiS->SiS_Pr->CCRT1CRTC[8] =  (pSiS->SiS_Pr->CVSyncStart     ) & 0xFF;		/* cr10 */
   pSiS->SiS_Pr->CCRT1CRTC[9] =  ((pSiS->SiS_Pr->CVSyncEnd      ) & 0x0F) | 0x80;	/* cr11 */
   pSiS->SiS_Pr->CCRT1CRTC[10] = (pSiS->SiS_Pr->CVDisplay    - 1) & 0xFF;		/* cr12 */
   pSiS->SiS_Pr->CCRT1CRTC[11] = (pSiS->SiS_Pr->CVBlankStart - 1) & 0xFF;		/* cr15 */
   pSiS->SiS_Pr->CCRT1CRTC[12] = (pSiS->SiS_Pr->CVBlankEnd   - 1) & 0xFF;		/* cr16 */

   pSiS->SiS_Pr->CCRT1CRTC[13] =
                        GETBITSTR((pSiS->SiS_Pr->CVTotal     -2), 10:10, 0:0) |
                        GETBITSTR((pSiS->SiS_Pr->CVDisplay   -1), 10:10, 1:1) |
                        GETBITSTR((pSiS->SiS_Pr->CVBlankStart-1), 10:10, 2:2) |
                        GETBITSTR((pSiS->SiS_Pr->CVSyncStart   ), 10:10, 3:3) |
                        GETBITSTR((pSiS->SiS_Pr->CVBlankEnd  -1),   8:8, 4:4) |
                        GETBITSTR((pSiS->SiS_Pr->CVSyncEnd     ),   4:4, 5:5) ;

   pSiS->SiS_Pr->CCRT1CRTC[14] =
                        GETBITSTR((pSiS->SiS_Pr->CHTotal      >> 3) - 5, 9:8, 1:0) |
                        GETBITSTR((pSiS->SiS_Pr->CHDisplay    >> 3) - 1, 9:8, 3:2) |
                        GETBITSTR((pSiS->SiS_Pr->CHBlankStart >> 3) - 1, 9:8, 5:4) |
                        GETBITSTR((pSiS->SiS_Pr->CHSyncStart  >> 3) + 3, 9:8, 7:6) ;


   pSiS->SiS_Pr->CCRT1CRTC[15] =
                        GETBITSTR((pSiS->SiS_Pr->CHBlankEnd >> 3) - 1, 7:6, 1:0) |
                        GETBITSTR((pSiS->SiS_Pr->CHSyncEnd  >> 3) + 3, 5:5, 2:2) ;

   switch(depth) {
   case 8:
      	pSiS->SiS_Pr->CModeFlag |= 0x223b;
	break;
   case 16:
      	pSiS->SiS_Pr->CModeFlag |= 0x227d;
	break;
   case 32:
      	pSiS->SiS_Pr->CModeFlag |= 0x22ff;
	break;		
   default: 
   	return 0;	
   }	
   
   if(pSiS->SiS_Pr->CFlags & V_DBLSCAN) 
   	pSiS->SiS_Pr->CModeFlag |= DoubleScanMode;
   if((pSiS->SiS_Pr->CVDisplay >= 1024)	|| 
      (pSiS->SiS_Pr->CVTotal >= 1024)   || 
      (pSiS->SiS_Pr->CHDisplay >= 1024))
	pSiS->SiS_Pr->CModeFlag |= LineCompareOff;
   if(pSiS->SiS_Pr->CFlags & V_CLKDIV2)
        pSiS->SiS_Pr->CModeFlag |= HalfDCLK;

   pSiS->SiS_Pr->CInfoFlag = 0x0007;
   if(pSiS->SiS_Pr->CFlags & V_NHSYNC)
   	pSiS->SiS_Pr->CInfoFlag |= 0x4000;
   if(pSiS->SiS_Pr->CFlags & V_NVSYNC) 
   	pSiS->SiS_Pr->CInfoFlag |= 0x8000;
   if(pSiS->SiS_Pr->CFlags & V_INTERLACE)	
	pSiS->SiS_Pr->CInfoFlag |= InterlaceMode;

   pSiS->SiS_Pr->UseCustomMode = TRUE;
#ifdef TWDEBUG
   xf86DrvMsg(0, X_INFO, "Custom mode %dx%d:\n", 
   	pSiS->SiS_Pr->CHDisplay,pSiS->SiS_Pr->CVDisplay);
   xf86DrvMsg(0, X_INFO, "Modeflag %04x, Infoflag %04x\n",
   	pSiS->SiS_Pr->CModeFlag, pSiS->SiS_Pr->CInfoFlag);
   xf86DrvMsg(0, X_INFO, " {{0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,\n",
   	pSiS->SiS_Pr->CCRT1CRTC[0],
	pSiS->SiS_Pr->CCRT1CRTC[1],
	pSiS->SiS_Pr->CCRT1CRTC[2],
	pSiS->SiS_Pr->CCRT1CRTC[3],
	pSiS->SiS_Pr->CCRT1CRTC[4],
	pSiS->SiS_Pr->CCRT1CRTC[5],
	pSiS->SiS_Pr->CCRT1CRTC[6],
	pSiS->SiS_Pr->CCRT1CRTC[7]);
   xf86DrvMsg(0, X_INFO, "  0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,\n",
   	pSiS->SiS_Pr->CCRT1CRTC[8],
	pSiS->SiS_Pr->CCRT1CRTC[9],
	pSiS->SiS_Pr->CCRT1CRTC[10],
	pSiS->SiS_Pr->CCRT1CRTC[11],
	pSiS->SiS_Pr->CCRT1CRTC[12],
	pSiS->SiS_Pr->CCRT1CRTC[13],
	pSiS->SiS_Pr->CCRT1CRTC[14],
	pSiS->SiS_Pr->CCRT1CRTC[15]);
   xf86DrvMsg(0, X_INFO, "  0x%02x}},\n", pSiS->SiS_Pr->CCRT1CRTC[16]);
   xf86DrvMsg(0, X_INFO, "Clock: 0x%02x, 0x%02x, %d\n",
   	pSiS->SiS_Pr->CSR2B,
	pSiS->SiS_Pr->CSR2C,
	pSiS->SiS_Pr->CSRClock);
#endif
   return 1;
}

/* TW: Build a list of supported modes */
DisplayModePtr
SiSBuildBuiltInModeList(ScrnInfoPtr pScrn, BOOLEAN includelcdmodes, BOOLEAN isfordvi)
{
   SISPtr         pSiS = SISPTR(pScrn);
   unsigned short VRE, VBE, VRS, VBS, VDE, VT;
   unsigned short HRE, HBE, HRS, HBS, HDE, HT;
   unsigned char  sr_data, cr_data, cr_data2, cr_data3;
   unsigned char  sr2b, sr2c;
   float          num, denum, postscalar, divider;
   int            A, B, C, D, E, F, temp, i, j, k, l, index, vclkindex;
   DisplayModePtr new = NULL, current = NULL, first = NULL;
   BOOLEAN        done = FALSE;
#if 0
   DisplayModePtr backup = NULL;
#endif

   pSiS->backupmodelist = NULL;
   pSiS->AddedPlasmaModes = FALSE;

   /* Initialize our pointers */
   if(pSiS->VGAEngine == SIS_300_VGA) {
#ifdef SIS300
	InitTo300Pointer(pSiS->SiS_Pr, &pSiS->sishw_ext);
#else
	return NULL;
#endif
   } else if(pSiS->VGAEngine == SIS_315_VGA) {
#ifdef SIS315H
       	InitTo310Pointer(pSiS->SiS_Pr, &pSiS->sishw_ext);
#else
	return NULL;
#endif
   } else return NULL;

   i = 0;
   while(pSiS->SiS_Pr->SiS_RefIndex[i].Ext_InfoFlag != 0xFFFF) {

      index = pSiS->SiS_Pr->SiS_RefIndex[i].Ext_CRT1CRTC;
#if 0 /* Not any longer */    
      if(pSiS->VGAEngine == SIS_300_VGA) index &= 0x3F;
#endif      

      /* 0x5a (320x240) is a pure FTSN mode, not DSTN! */
      if((!pSiS->FSTN) &&
	 (pSiS->SiS_Pr->SiS_RefIndex[i].ModeID == 0x5a))  {
           i++;
      	   continue;
      }
      if((pSiS->FSTN) &&
         (pSiS->SiS_Pr->SiS_RefIndex[i].XRes == 320) &&
	 (pSiS->SiS_Pr->SiS_RefIndex[i].YRes == 240) &&
	 (pSiS->SiS_Pr->SiS_RefIndex[i].ModeID != 0x5a)) {
	   i++;
	   continue;
      }

      if(!(new = xalloc(sizeof(DisplayModeRec)))) return first;
      memset(new, 0, sizeof(DisplayModeRec));
      if(!(new->name = xalloc(10))) {
      		xfree(new);
		return first;
      }
      if(!first) first = new;
      if(current) {
         current->next = new;
	 new->prev = current;
      }

      current = new;

      sprintf(current->name, "%dx%d", pSiS->SiS_Pr->SiS_RefIndex[i].XRes,
                                      pSiS->SiS_Pr->SiS_RefIndex[i].YRes);

      current->status = MODE_OK;

      current->type = M_T_DEFAULT;

      vclkindex = pSiS->SiS_Pr->SiS_RefIndex[i].Ext_CRTVCLK;
      if(pSiS->VGAEngine == SIS_300_VGA) vclkindex &= 0x3F;

      sr2b = pSiS->SiS_Pr->SiS_VCLKData[vclkindex].SR2B;
      sr2c = pSiS->SiS_Pr->SiS_VCLKData[vclkindex].SR2C;

      divider = (sr2b & 0x80) ? 2.0 : 1.0;
      postscalar = (sr2c & 0x80) ?
              ( (((sr2c >> 5) & 0x03) == 0x02) ? 6.0 : 8.0) : (((sr2c >> 5) & 0x03) + 1.0);
      num = (sr2b & 0x7f) + 1.0;
      denum = (sr2c & 0x1f) + 1.0;

#ifdef TWDEBUG
      xf86DrvMsg(0, X_INFO, "------------\n");
      xf86DrvMsg(0, X_INFO, "sr2b: %x sr2c %x div %f ps %f num %f denum %f\n",
         sr2b, sr2c, divider, postscalar, num, denum);
#endif

      current->Clock = (int)(14318 * (divider / postscalar) * (num / denum));

      sr_data = pSiS->SiS_Pr->SiS_CRT1Table[index].CR[14];
	/* inSISIDXREG(SISSR, 0x0b, sr_data); */

      cr_data = pSiS->SiS_Pr->SiS_CRT1Table[index].CR[0];
	/* inSISIDXREG(SISCR, 0x00, cr_data); */

      /* Horizontal total */
      HT = (cr_data & 0xff) |
           ((unsigned short) (sr_data & 0x03) << 8);
      A = HT + 5;

      cr_data = pSiS->SiS_Pr->SiS_CRT1Table[index].CR[1];
	/* inSISIDXREG(SISCR, 0x01, cr_data); */

      /* Horizontal display enable end */
      HDE = (cr_data & 0xff) |
            ((unsigned short) (sr_data & 0x0C) << 6);
      E = HDE + 1;

      cr_data = pSiS->SiS_Pr->SiS_CRT1Table[index].CR[4];
	/* inSISIDXREG(SISCR, 0x04, cr_data); */

      /* Horizontal retrace (=sync) start */
      HRS = (cr_data & 0xff) |
            ((unsigned short) (sr_data & 0xC0) << 2);
      F = HRS - E - 3;

      cr_data = pSiS->SiS_Pr->SiS_CRT1Table[index].CR[2];
	/* inSISIDXREG(SISCR, 0x02, cr_data); */

      /* Horizontal blank start */
      HBS = (cr_data & 0xff) |
            ((unsigned short) (sr_data & 0x30) << 4);

      sr_data = pSiS->SiS_Pr->SiS_CRT1Table[index].CR[15];
	/* inSISIDXREG(SISSR, 0x0c, sr_data); */

      cr_data = pSiS->SiS_Pr->SiS_CRT1Table[index].CR[3];
	/* inSISIDXREG(SISCR, 0x03, cr_data);  */

      cr_data2 = pSiS->SiS_Pr->SiS_CRT1Table[index].CR[5];
	/* inSISIDXREG(SISCR, 0x05, cr_data2); */

      /* Horizontal blank end */
      HBE = (cr_data & 0x1f) |
            ((unsigned short) (cr_data2 & 0x80) >> 2) |
	    ((unsigned short) (sr_data & 0x03) << 6);

      /* Horizontal retrace (=sync) end */
      HRE = (cr_data2 & 0x1f) | ((sr_data & 0x04) << 3);

      temp = HBE - ((E - 1) & 255);
      B = (temp > 0) ? temp : (temp + 256);

      temp = HRE - ((E + F + 3) & 63);
      C = (temp > 0) ? temp : (temp + 64);

      D = B - F - C;

      if((pSiS->SiS_Pr->SiS_RefIndex[i].XRes == 320) &&
	 ((pSiS->SiS_Pr->SiS_RefIndex[i].YRes == 200) ||
	  (pSiS->SiS_Pr->SiS_RefIndex[i].YRes == 240))) {

	 /* Terrible hack, but correct CRTC data for
	  * these modes only produces a black screen...
	  * (HRE is 0, leading into a too large C and
	  * a negative D. The CRT controller does not
	  * seem to like correcting HRE to 50
	  */
	 current->HDisplay   = 320;
         current->HSyncStart = 328;
         current->HSyncEnd   = 376;
         current->HTotal     = 400;

      } else {

         current->HDisplay   = (E * 8);
         current->HSyncStart = (E * 8) + (F * 8);
         current->HSyncEnd   = (E * 8) + (F * 8) + (C * 8);
         current->HTotal     = (E * 8) + (F * 8) + (C * 8) + (D * 8);

      }

#ifdef TWDEBUG
      xf86DrvMsg(0, X_INFO,
        "H: A %d B %d C %d D %d E %d F %d  HT %d HDE %d HRS %d HBS %d HBE %d HRE %d\n",
      	A, B, C, D, E, F, HT, HDE, HRS, HBS, HBE, HRE);
#endif

      sr_data = pSiS->SiS_Pr->SiS_CRT1Table[index].CR[13];
	/* inSISIDXREG(SISSR, 0x0A, sr_data); */

      cr_data = pSiS->SiS_Pr->SiS_CRT1Table[index].CR[6];
        /* inSISIDXREG(SISCR, 0x06, cr_data); */

      cr_data2 = pSiS->SiS_Pr->SiS_CRT1Table[index].CR[7];
        /* inSISIDXREG(SISCR, 0x07, cr_data2);  */

      /* Vertical total */
      VT = (cr_data & 0xFF) |
           ((unsigned short) (cr_data2 & 0x01) << 8) |
	   ((unsigned short)(cr_data2 & 0x20) << 4) |
	   ((unsigned short) (sr_data & 0x01) << 10);
      A = VT + 2;

      cr_data = pSiS->SiS_Pr->SiS_CRT1Table[index].CR[10];
	/* inSISIDXREG(SISCR, 0x12, cr_data);  */

      /* Vertical display enable end */
      VDE = (cr_data & 0xff) |
            ((unsigned short) (cr_data2 & 0x02) << 7) |
	    ((unsigned short) (cr_data2 & 0x40) << 3) |
	    ((unsigned short) (sr_data & 0x02) << 9);
      E = VDE + 1;

      cr_data = pSiS->SiS_Pr->SiS_CRT1Table[index].CR[8];
	/* inSISIDXREG(SISCR, 0x10, cr_data); */

      /* Vertical retrace (=sync) start */
      VRS = (cr_data & 0xff) |
            ((unsigned short) (cr_data2 & 0x04) << 6) |
	    ((unsigned short) (cr_data2 & 0x80) << 2) |
	    ((unsigned short) (sr_data & 0x08) << 7);
      F = VRS + 1 - E;

      cr_data =  pSiS->SiS_Pr->SiS_CRT1Table[index].CR[11];
	/* inSISIDXREG(SISCR, 0x15, cr_data);  */

      cr_data3 = (pSiS->SiS_Pr->SiS_CRT1Table[index].CR[16] & 0x01) << 5;
	/* inSISIDXREG(SISCR, 0x09, cr_data3);  */

      /* Vertical blank start */
      VBS = (cr_data & 0xff) |
            ((unsigned short) (cr_data2 & 0x08) << 5) |
	    ((unsigned short) (cr_data3 & 0x20) << 4) |
	    ((unsigned short) (sr_data & 0x04) << 8);

      cr_data =  pSiS->SiS_Pr->SiS_CRT1Table[index].CR[12];
	/* inSISIDXREG(SISCR, 0x16, cr_data); */

      /* Vertical blank end */
      VBE = (cr_data & 0xff) |
            ((unsigned short) (sr_data & 0x10) << 4);
      temp = VBE - ((E - 1) & 511);
      B = (temp > 0) ? temp : (temp + 512);

      cr_data = pSiS->SiS_Pr->SiS_CRT1Table[index].CR[9];
	/* inSISIDXREG(SISCR, 0x11, cr_data); */

      /* Vertical retrace (=sync) end */
      VRE = (cr_data & 0x0f) | ((sr_data & 0x20) >> 1);
      temp = VRE - ((E + F - 1) & 31);
      C = (temp > 0) ? temp : (temp + 32);

      D = B - F - C;

      current->VDisplay   = VDE + 1;
      current->VSyncStart = VRS + 1;
      current->VSyncEnd   = ((VRS & ~0x1f) | VRE) + 1;
      if(VRE <= (VRS & 0x1f)) current->VSyncEnd += 32;
      current->VTotal     = E + D + C + F;

#if 0
      current->VDisplay   = E;
      current->VSyncStart = E + D;
      current->VSyncEnd   = E + D + C;
      current->VTotal     = E + D + C + F;
#endif

#ifdef TWDEBUG
      xf86DrvMsg(0, X_INFO,
        "V: A %d B %d C %d D %d E %d F %d  VT %d VDE %d VRS %d VBS %d VBE %d VRE %d\n",
      	A, B, C, D, E, F, VT, VDE, VRS, VBS, VBE, VRE);
#endif

      if(pSiS->SiS_Pr->SiS_RefIndex[i].Ext_InfoFlag & 0x4000)
          current->Flags |= V_NHSYNC;
      else
          current->Flags |= V_PHSYNC;

      if(pSiS->SiS_Pr->SiS_RefIndex[i].Ext_InfoFlag & 0x8000)
      	  current->Flags |= V_NVSYNC;
      else
          current->Flags |= V_PVSYNC;

      if(pSiS->SiS_Pr->SiS_RefIndex[i].Ext_InfoFlag & 0x0080)
          current->Flags |= V_INTERLACE;

      j = 0;
      while(pSiS->SiS_Pr->SiS_EModeIDTable[j].Ext_ModeID != 0xff) {
          if(pSiS->SiS_Pr->SiS_EModeIDTable[j].Ext_ModeID ==
	                  pSiS->SiS_Pr->SiS_RefIndex[i].ModeID) {
              if(pSiS->SiS_Pr->SiS_EModeIDTable[j].Ext_ModeFlag & DoubleScanMode) {
	      	  current->Flags |= V_DBLSCAN;
              }
	      break;
          }
	  j++;
      }

      if(current->Flags & V_INTERLACE) {
         current->VDisplay <<= 1;
	 current->VSyncStart <<= 1;
	 current->VSyncEnd <<= 1;
	 current->VTotal <<= 1;
	 current->VTotal |= 1;
      }
      if(current->Flags & V_DBLSCAN) {
         current->Clock >>= 1;
	 current->VDisplay >>= 1;
	 current->VSyncStart >>= 1;
	 current->VSyncEnd >>= 1;
	 current->VTotal >>= 1;
      }

#if 0
      if((backup = xalloc(sizeof(DisplayModeRec)))) {
         if(!pSiS->backupmodelist) pSiS->backupmodelist = backup;
	 else {
	    pSiS->backupmodelist->next = backup;
	    backup->prev = pSiS->backupmodelist;
	 }
	 backup->next = NULL;
	 backup->HDisplay = current->HDisplay;
         backup->HSyncStart = current->HSyncStart;
         backup->HSyncEnd = current->HSyncEnd;
         backup->HTotal = current->HTotal;
         backup->VDisplay = current->VDisplay;
         backup->VSyncStart = current->VSyncStart;
         backup->VSyncEnd = current->VSyncEnd;
         backup->VTotal = current->VTotal;
	 backup->Flags = current->Flags;
	 backup->Clock = current->Clock;
      }
#endif

#ifdef TWDEBUG
      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
      	"Built-in: %s %.2f %d %d %d %d %d %d %d %d\n",
	current->name, (float)current->Clock / 1000,
	current->HDisplay, current->HSyncStart, current->HSyncEnd, current->HTotal,
	current->VDisplay, current->VSyncStart, current->VSyncEnd, current->VTotal);
#endif

      i++;
   }

   /* Add non-standard LCD modes for panel's detailed timings */

   if(!includelcdmodes) return first;

   xf86DrvMsg(0, X_INFO, "Checking database for vendor %x, product %x\n",
      pSiS->SiS_Pr->CP_Vendor, pSiS->SiS_Pr->CP_Product);

   i = 0;
   while((!done) && (SiS_PlasmaTable[i].vendor) && (pSiS->SiS_Pr->CP_Vendor)) {

     if(SiS_PlasmaTable[i].vendor == pSiS->SiS_Pr->CP_Vendor) {

        for(j=0; j<SiS_PlasmaTable[i].productnum; j++) {

	    if(SiS_PlasmaTable[i].product[j] == pSiS->SiS_Pr->CP_Product) {

	       xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	       	  "Identified %s panel, adding specific modes\n",
		  SiS_PlasmaTable[i].plasmaname);

	       for(k=0; k<SiS_PlasmaTable[i].modenum; k++) {

	          if(isfordvi) {
		     if(!(SiS_PlasmaTable[i].plasmamodes[k] & 0x80)) continue;
		  } else {
		     if(!(SiS_PlasmaTable[i].plasmamodes[k] & 0x40)) continue;
		  }

	          if(!(new = xalloc(sizeof(DisplayModeRec)))) return first;

                  memset(new, 0, sizeof(DisplayModeRec));
                  if(!(new->name = xalloc(10))) {
      		     xfree(new);
		     return first;
                  }
                  if(!first) first = new;
                  if(current) {
                     current->next = new;
	             new->prev = current;
                  }

                  current = new;

		  pSiS->AddedPlasmaModes = TRUE;

		  l = SiS_PlasmaTable[i].plasmamodes[k] & 0x3f;

	          sprintf(current->name, "%dx%d", SiS_PlasmaMode[l].HDisplay,
                                                  SiS_PlasmaMode[l].VDisplay);

                  current->status = MODE_OK;

                  current->type = M_T_BUILTIN;

		  current->Clock = SiS_PlasmaMode[l].clock;
            	  current->SynthClock = current->Clock;

                  current->HDisplay   = SiS_PlasmaMode[l].HDisplay;
                  current->HSyncStart = current->HDisplay + SiS_PlasmaMode[l].HFrontPorch;
                  current->HSyncEnd   = current->HSyncStart + SiS_PlasmaMode[l].HSyncWidth;
                  current->HTotal     = SiS_PlasmaMode[l].HTotal;

		  current->VDisplay   = SiS_PlasmaMode[l].VDisplay;
                  current->VSyncStart = current->VDisplay + SiS_PlasmaMode[l].VFrontPorch;
                  current->VSyncEnd   = current->VSyncStart + SiS_PlasmaMode[l].VSyncWidth;
                  current->VTotal     = SiS_PlasmaMode[l].VTotal;

                  current->CrtcHDisplay = current->HDisplay;
                  current->CrtcHBlankStart = current->HSyncStart;
                  current->CrtcHSyncStart = current->HSyncStart;
                  current->CrtcHSyncEnd = current->HSyncEnd;
                  current->CrtcHBlankEnd = current->HSyncEnd;
                  current->CrtcHTotal = current->HTotal;

                  current->CrtcVDisplay = current->VDisplay;
                  current->CrtcVBlankStart = current->VSyncStart;
                  current->CrtcVSyncStart = current->VSyncStart;
                  current->CrtcVSyncEnd = current->VSyncEnd;
                  current->CrtcVBlankEnd = current->VSyncEnd;
                  current->CrtcVTotal = current->VTotal;

                  if(SiS_PlasmaMode[l].SyncFlags & SIS_PL_HSYNCP)
                     current->Flags |= V_PHSYNC;
                  else
                     current->Flags |= V_NHSYNC;

                  if(SiS_PlasmaMode[l].SyncFlags & SIS_PL_VSYNCP)
                     current->Flags |= V_PVSYNC;
                  else
                     current->Flags |= V_NVSYNC;

		  if(current->HDisplay > pSiS->LCDwidth)
		     pSiS->LCDwidth = pSiS->SiS_Pr->CP_MaxX = current->HDisplay;
	          if(current->VDisplay > pSiS->LCDheight)
		     pSiS->LCDheight = pSiS->SiS_Pr->CP_MaxY = current->VDisplay;

               }
	       done = TRUE;
	       break;
	    }
	}
     }

     i++;

   }

   if(pSiS->SiS_Pr->CP_HaveCustomData) {

      for(i=0; i<7; i++) {

         if(pSiS->SiS_Pr->CP_DataValid[i]) {

            if(!(new = xalloc(sizeof(DisplayModeRec)))) return first;

            memset(new, 0, sizeof(DisplayModeRec));
            if(!(new->name = xalloc(10))) {
      		xfree(new);
		return first;
            }
            if(!first) first = new;
            if(current) {
               current->next = new;
	       new->prev = current;
            }

            current = new;

            sprintf(current->name, "%dx%d", pSiS->SiS_Pr->CP_HDisplay[i],
                                            pSiS->SiS_Pr->CP_VDisplay[i]);

            current->status = MODE_OK;

            current->type = M_T_BUILTIN;

            current->Clock = pSiS->SiS_Pr->CP_Clock[i];
            current->SynthClock = current->Clock;

            current->HDisplay   = pSiS->SiS_Pr->CP_HDisplay[i];
            current->HSyncStart = pSiS->SiS_Pr->CP_HSyncStart[i];
            current->HSyncEnd   = pSiS->SiS_Pr->CP_HSyncEnd[i];
            current->HTotal     = pSiS->SiS_Pr->CP_HTotal[i];

            current->VDisplay   = pSiS->SiS_Pr->CP_VDisplay[i];
            current->VSyncStart = pSiS->SiS_Pr->CP_VSyncStart[i];
            current->VSyncEnd   = pSiS->SiS_Pr->CP_VSyncEnd[i];
            current->VTotal     = pSiS->SiS_Pr->CP_VTotal[i];

            current->CrtcHDisplay = current->HDisplay;
            current->CrtcHBlankStart = pSiS->SiS_Pr->CP_HBlankStart[i];
            current->CrtcHSyncStart = current->HSyncStart;
            current->CrtcHSyncEnd = current->HSyncEnd;
            current->CrtcHBlankEnd = pSiS->SiS_Pr->CP_HBlankEnd[i];
            current->CrtcHTotal = current->HTotal;

            current->CrtcVDisplay = current->VDisplay;
            current->CrtcVBlankStart = pSiS->SiS_Pr->CP_VBlankStart[i];
            current->CrtcVSyncStart = current->VSyncStart;
            current->CrtcVSyncEnd = current->VSyncEnd;
            current->CrtcVBlankEnd = pSiS->SiS_Pr->CP_VBlankEnd[i];
            current->CrtcVTotal = current->VTotal;

	    if(pSiS->SiS_Pr->CP_SyncValid[i]) {
               if(pSiS->SiS_Pr->CP_HSync_P[i])
                  current->Flags |= V_PHSYNC;
               else
                  current->Flags |= V_NHSYNC;

               if(pSiS->SiS_Pr->CP_VSync_P[i])
                  current->Flags |= V_PVSYNC;
               else
                  current->Flags |= V_NVSYNC;
	    } else {
	       /* No sync data? Use positive sync... */
	       current->Flags |= V_PHSYNC;
	       current->Flags |= V_PVSYNC;
	    }
         }
      }
   }

   return first;

}
#endif

#ifdef LINUX_KERNEL
int
sisfb_mode_rate_to_dclock(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension,
			  unsigned char modeno, unsigned char rateindex)
{
    USHORT ModeNo = modeno;
    USHORT ModeIdIndex = 0, ClockIndex = 0;
    USHORT RefreshRateTableIndex = 0;
    UCHAR  *ROMAddr  = HwDeviceExtension->pjVirtualRomBase;
    ULONG  temp = 0;
    int    Clock;

    if(HwDeviceExtension->jChipType < SIS_315H) {
#ifdef SIS300
       InitTo300Pointer(SiS_Pr, HwDeviceExtension);
#else
       return 65 * 1000 * 1000;
#endif
    } else {
#ifdef SIS315H
       InitTo310Pointer(SiS_Pr, HwDeviceExtension);
#else
       return 65 * 1000 * 1000;
#endif
    }

    temp = SiS_SearchModeID(SiS_Pr, ROMAddr, &ModeNo, &ModeIdIndex);
    if(!temp) {
    	printk(KERN_ERR "Could not find mode %x\n", ModeNo);
    	return 65 * 1000 * 1000;
    }
    
    RefreshRateTableIndex = SiS_Pr->SiS_EModeIDTable[ModeIdIndex].REFindex;
    RefreshRateTableIndex += (rateindex - 1);
    ClockIndex = SiS_Pr->SiS_RefIndex[RefreshRateTableIndex].Ext_CRTVCLK;
    if(HwDeviceExtension->jChipType < SIS_315H) {
       ClockIndex &= 0x3F;
    }
    Clock = SiS_Pr->SiS_VCLKData[ClockIndex].CLOCK * 1000 * 1000;
    
    return(Clock);
}

BOOLEAN
sisfb_gettotalfrommode(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension,
		       unsigned char modeno, int *htotal, int *vtotal, unsigned char rateindex)
{
    USHORT ModeNo = modeno;
    USHORT ModeIdIndex = 0, CRT1Index = 0;
    USHORT RefreshRateTableIndex = 0;
    UCHAR  *ROMAddr  = HwDeviceExtension->pjVirtualRomBase;
    ULONG  temp = 0;
    unsigned char  sr_data, cr_data, cr_data2;

    if(HwDeviceExtension->jChipType < SIS_315H) {
#ifdef SIS300
       InitTo300Pointer(SiS_Pr, HwDeviceExtension);
#else
       return FALSE;
#endif
    } else {
#ifdef SIS315H
       InitTo310Pointer(SiS_Pr, HwDeviceExtension);
#else
       return FALSE;
#endif
    }

    temp = SiS_SearchModeID(SiS_Pr, ROMAddr, &ModeNo, &ModeIdIndex);
    if(!temp) return FALSE;

    RefreshRateTableIndex = SiS_Pr->SiS_EModeIDTable[ModeIdIndex].REFindex;
    RefreshRateTableIndex += (rateindex - 1);
    CRT1Index = SiS_Pr->SiS_RefIndex[RefreshRateTableIndex].Ext_CRT1CRTC;

    sr_data = SiS_Pr->SiS_CRT1Table[CRT1Index].CR[14];
    cr_data = SiS_Pr->SiS_CRT1Table[CRT1Index].CR[0];
    *htotal = (((cr_data & 0xff) | ((unsigned short) (sr_data & 0x03) << 8)) + 5) * 8;

    sr_data = SiS_Pr->SiS_CRT1Table[CRT1Index].CR[13];
    cr_data = SiS_Pr->SiS_CRT1Table[CRT1Index].CR[6];
    cr_data2 = SiS_Pr->SiS_CRT1Table[CRT1Index].CR[7];
    *vtotal = ((cr_data & 0xFF) |
               ((unsigned short)(cr_data2 & 0x01) <<  8) |
	       ((unsigned short)(cr_data2 & 0x20) <<  4) |
	       ((unsigned short)(sr_data  & 0x01) << 10)) + 2;

    if(SiS_Pr->SiS_RefIndex[RefreshRateTableIndex].Ext_InfoFlag & InterlaceMode)
       *vtotal *= 2;

    return TRUE;
}

int
sisfb_mode_rate_to_ddata(SiS_Private *SiS_Pr, PSIS_HW_DEVICE_INFO HwDeviceExtension,
			 unsigned char modeno, unsigned char rateindex,
			 ULONG *left_margin, ULONG *right_margin, 
			 ULONG *upper_margin, ULONG *lower_margin,
			 ULONG *hsync_len, ULONG *vsync_len,
			 ULONG *sync, ULONG *vmode)
{
    USHORT ModeNo = modeno;
    USHORT ModeIdIndex = 0, index = 0;
    USHORT RefreshRateTableIndex = 0;
    UCHAR  *ROMAddr  = HwDeviceExtension->pjVirtualRomBase;
    unsigned short VRE, VBE, VRS, VBS, VDE, VT;
    unsigned short HRE, HBE, HRS, HBS, HDE, HT;
    unsigned char  sr_data, cr_data, cr_data2, cr_data3;
    int            A, B, C, D, E, F, temp, j;
   
    if(HwDeviceExtension->jChipType < SIS_315H) {
#ifdef SIS300
       InitTo300Pointer(SiS_Pr, HwDeviceExtension);
#else
       return 0;
#endif
    } else {
#ifdef SIS315H
       InitTo310Pointer(SiS_Pr, HwDeviceExtension);
#else
       return 0;
#endif
    }
    
    temp = SiS_SearchModeID(SiS_Pr, ROMAddr, &ModeNo, &ModeIdIndex);
    if(!temp) return 0;
    
    RefreshRateTableIndex = SiS_Pr->SiS_EModeIDTable[ModeIdIndex].REFindex;
    RefreshRateTableIndex += (rateindex - 1);
    index = SiS_Pr->SiS_RefIndex[RefreshRateTableIndex].Ext_CRT1CRTC;

    sr_data = SiS_Pr->SiS_CRT1Table[index].CR[14];

    cr_data = SiS_Pr->SiS_CRT1Table[index].CR[0];

    /* Horizontal total */
    HT = (cr_data & 0xff) |
         ((unsigned short) (sr_data & 0x03) << 8);
    A = HT + 5;

    cr_data = SiS_Pr->SiS_CRT1Table[index].CR[1];
	
    /* Horizontal display enable end */
    HDE = (cr_data & 0xff) |
          ((unsigned short) (sr_data & 0x0C) << 6);
    E = HDE + 1;

    cr_data = SiS_Pr->SiS_CRT1Table[index].CR[4];
	
    /* Horizontal retrace (=sync) start */
    HRS = (cr_data & 0xff) |
          ((unsigned short) (sr_data & 0xC0) << 2);
    F = HRS - E - 3;

    cr_data = SiS_Pr->SiS_CRT1Table[index].CR[2];
	
    /* Horizontal blank start */
    HBS = (cr_data & 0xff) |
          ((unsigned short) (sr_data & 0x30) << 4);

    sr_data = SiS_Pr->SiS_CRT1Table[index].CR[15];
	
    cr_data = SiS_Pr->SiS_CRT1Table[index].CR[3];

    cr_data2 = SiS_Pr->SiS_CRT1Table[index].CR[5];
	
    /* Horizontal blank end */
    HBE = (cr_data & 0x1f) |
          ((unsigned short) (cr_data2 & 0x80) >> 2) |
	  ((unsigned short) (sr_data & 0x03) << 6);

    /* Horizontal retrace (=sync) end */
    HRE = (cr_data2 & 0x1f) | ((sr_data & 0x04) << 3);

    temp = HBE - ((E - 1) & 255);
    B = (temp > 0) ? temp : (temp + 256);

    temp = HRE - ((E + F + 3) & 63);
    C = (temp > 0) ? temp : (temp + 64);

    D = B - F - C;

    if((SiS_Pr->SiS_RefIndex[RefreshRateTableIndex].XRes == 320) &&
       ((SiS_Pr->SiS_RefIndex[RefreshRateTableIndex].YRes == 200) ||
	(SiS_Pr->SiS_RefIndex[RefreshRateTableIndex].YRes == 240))) {

	 /* Terrible hack, but the correct CRTC data for
	  * these modes only produces a black screen...
	  */
       *left_margin = (400 - 376);
       *right_margin = (328 - 320);
       *hsync_len = (376 - 328);

    } else {

       *left_margin = D * 8;
       *right_margin = F * 8;
       *hsync_len = C * 8;

    }

    sr_data = SiS_Pr->SiS_CRT1Table[index].CR[13];

    cr_data = SiS_Pr->SiS_CRT1Table[index].CR[6];

    cr_data2 = SiS_Pr->SiS_CRT1Table[index].CR[7];

    /* Vertical total */
    VT = (cr_data & 0xFF) |
         ((unsigned short) (cr_data2 & 0x01) << 8) |
	 ((unsigned short)(cr_data2 & 0x20) << 4) |
	 ((unsigned short) (sr_data & 0x01) << 10);
    A = VT + 2;

    cr_data = SiS_Pr->SiS_CRT1Table[index].CR[10];
	
    /* Vertical display enable end */
    VDE = (cr_data & 0xff) |
          ((unsigned short) (cr_data2 & 0x02) << 7) |
	  ((unsigned short) (cr_data2 & 0x40) << 3) |
	  ((unsigned short) (sr_data & 0x02) << 9);
    E = VDE + 1;

    cr_data = SiS_Pr->SiS_CRT1Table[index].CR[8];

    /* Vertical retrace (=sync) start */
    VRS = (cr_data & 0xff) |
          ((unsigned short) (cr_data2 & 0x04) << 6) |
	  ((unsigned short) (cr_data2 & 0x80) << 2) |
	  ((unsigned short) (sr_data & 0x08) << 7);
    F = VRS + 1 - E;

    cr_data =  SiS_Pr->SiS_CRT1Table[index].CR[11];

    cr_data3 = (SiS_Pr->SiS_CRT1Table[index].CR[16] & 0x01) << 5;

    /* Vertical blank start */
    VBS = (cr_data & 0xff) |
          ((unsigned short) (cr_data2 & 0x08) << 5) |
	  ((unsigned short) (cr_data3 & 0x20) << 4) |
	  ((unsigned short) (sr_data & 0x04) << 8);

    cr_data =  SiS_Pr->SiS_CRT1Table[index].CR[12];

    /* Vertical blank end */
    VBE = (cr_data & 0xff) |
          ((unsigned short) (sr_data & 0x10) << 4);
    temp = VBE - ((E - 1) & 511);
    B = (temp > 0) ? temp : (temp + 512);

    cr_data = SiS_Pr->SiS_CRT1Table[index].CR[9];

    /* Vertical retrace (=sync) end */
    VRE = (cr_data & 0x0f) | ((sr_data & 0x20) >> 1);
    temp = VRE - ((E + F - 1) & 31);
    C = (temp > 0) ? temp : (temp + 32);

    D = B - F - C;
      
    *upper_margin = D;
    *lower_margin = F;
    *vsync_len = C;

    if(SiS_Pr->SiS_RefIndex[RefreshRateTableIndex].Ext_InfoFlag & 0x8000)
       *sync &= ~FB_SYNC_VERT_HIGH_ACT;
    else
       *sync |= FB_SYNC_VERT_HIGH_ACT;

    if(SiS_Pr->SiS_RefIndex[RefreshRateTableIndex].Ext_InfoFlag & 0x4000)       
       *sync &= ~FB_SYNC_HOR_HIGH_ACT;
    else
       *sync |= FB_SYNC_HOR_HIGH_ACT;
		
    *vmode = FB_VMODE_NONINTERLACED;       
    if(SiS_Pr->SiS_RefIndex[RefreshRateTableIndex].Ext_InfoFlag & 0x0080)
       *vmode = FB_VMODE_INTERLACED;
    else {
      j = 0;
      while(SiS_Pr->SiS_EModeIDTable[j].Ext_ModeID != 0xff) {
          if(SiS_Pr->SiS_EModeIDTable[j].Ext_ModeID ==
	                  SiS_Pr->SiS_RefIndex[RefreshRateTableIndex].ModeID) {
              if(SiS_Pr->SiS_EModeIDTable[j].Ext_ModeFlag & DoubleScanMode) {
	      	  *vmode = FB_VMODE_DOUBLE;
              }
	      break;
          }
	  j++;
      }
    }       

    if((*vmode & FB_VMODE_MASK) == FB_VMODE_INTERLACED) {
#if 0  /* Do this? */
       *upper_margin <<= 1;
       *lower_margin <<= 1;
       *vsync_len <<= 1;
#endif
    } else if((*vmode & FB_VMODE_MASK) == FB_VMODE_DOUBLE) {
       *upper_margin >>= 1;
       *lower_margin >>= 1;
       *vsync_len >>= 1;
    }

    return 1;       
}			  

#endif

