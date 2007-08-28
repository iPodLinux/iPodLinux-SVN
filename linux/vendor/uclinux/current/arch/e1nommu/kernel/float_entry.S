/* File contains the floating point exceptions */

.global _ExceptionFCVTD_Entry
.global _ExceptionFCVT_Entry
.global _ExceptionFCMPUD_Entry
.global _ExceptionFCMPU_Entry
.global _ExceptionFCMPD_Entry
.global _ExceptionFCMP_Entry
.global _ExceptionFDIVD_Entry
.global _ExceptionFDIV_Entry
.global _ExceptionFMULD_Entry
.global _ExceptionFMUL_Entry
.global _ExceptionFSUBD_Entry
.global _ExceptionFSUB_Entry
.global _ExceptionFADDD_Entry
.global _ExceptionFADD_Entry

.balign 4
_ExceptionFCVTD_Entry:  movi PC, (long)FCVTDEntry
_ExceptionFCVT_Entry:   movi PC, (long)FCVTEntry
_ExceptionFCMPUD_Entry: movi PC, (long)FCMPUDEntry
_ExceptionFCMPU_Entry:  movi PC, (long)FCMPUEntry
_ExceptionFCMPD_Entry:  movi PC, (long)FCMPDEntry
_ExceptionFCMP_Entry:   movi PC, (long)FCMPEntry
_ExceptionFDIVD_Entry:  movi PC, (long)FDIVDEntry
_ExceptionFDIV_Entry:   movi PC, (long)FDIVEntry
_ExceptionFMULD_Entry:  movi PC, (long)FMULDEntry
_ExceptionFMUL_Entry:   movi PC, (long)FMULEntry
_ExceptionFSUBD_Entry:  movi PC, (long)FSUBDEntry
_ExceptionFSUB_Entry:   movi PC, (long)FSUBEntry
_ExceptionFADDD_Entry:  movi PC, (long)FADDDEntry
_ExceptionFADD_Entry:   movi PC, (long)FADDEntry

.global FloatErrorEntry
.balign 4
FloatErrorEntry:
;	FRAME L0, L0
;	CALL  L0, 0, _Handle_FP_Exception
	TRAP  46
	RET   PC, L3
