/******************************************************************************
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2000 Intel Corporation. All Rights Reserved.
//
//  VSS:
//		$Workfile: mp3decoder.h $
//      $Revision: 1.1 $
//      $Date: 2004/11/06 18:37:56 $
//      $Archive:   $
//
//  Description:
//      IPP MP3 Sample Decoder Header File
//
******************************************************************************/

/************************
 General Constants
*************************/
#define FALSE	0	/* General purpose binary flags */
#define TRUE	1
#define MAX_GRAN	2	/* Maximum number of granules per channel */
#define	MAX_CHAN	2	/* Maximum number of channels */
#define	SCF_BANDS	4	/* Number of scalefactor bands per channel */
#define	FIFO_THRESH	2880	/* Raw MP3 stream ring buffer (FIFO) refill threshold */
#define	HDR_BUF_SIZE	64	/* Raw MP3 stream header buffer */
#define	MAIN_DATA_BUF_SIZE	4096	/* Main data decoding buffer */
#define	STREAM_BUF_SIZE		65536	/* Raw bit stream buffer */

/************************
 MP3 Header Constants
*************************/
#define	MPEG_2_LSF	0	/* ISO IS13818-3, MPEG-2 LSF stream flag */
#define MPEG_1	1	/* ISO/IEC 11172-3, MPEG-1 stream flag */
#define MONO	3	/* Monaural mode flag */
			/* Must be radix-2 (2,4,8,16,...) (see notes below) */

/************************
 Decoder Return Codes
*************************/
#define	MP3_SYNC_NOT_FOUND	100	/* Sync word not found */
#define	MP3_BUFFER_UNDERRUN	101	/* Ring buffer contains insufficient data to decode current frame */
#define MP3_FRAME_UNDERRUN	102	/* Ring buffer OK, but main_data portion of current frame insufficient */
#define MP3_FRAME_HEADER_INVALID	103	/* Invalid MP3 frame header */
#define MP3_FRAME_COMPLETE	0	/* Frame decoding complete */

/***********************
 Decoder State Variable 
************************/
typedef struct {
	/***************************************
	 Data objects are grouped by primitive
	****************************************/

	/***********************************
	 1. ippsUnpackFrameHeader_MP3 
	 ***********************************/
	IppMP3FrameHeader FrameHdr;	/* MP3 frame header - gives bit rate, sample
					 * rate, # channels, etc. */
	Ipp8u HdrBuf[HDR_BUF_SIZE];	/* Buffer for decoding header word from MP3
					 * stream */
	Ipp8u *pHdrBuf;		/* Pointer to header decoding buffer - modified by
				 * primitive */

	/***********************************
	 2. ippsUnpackSideInfo_MP3 
	 ***********************************/
	IppMP3SideInfo SideInfo[MAX_GRAN][MAX_CHAN];	/* Table of MP3 side information
							 * - 1 or 2 granules, 1 or 2
							 * channels */

	/***********************************
	 3. ippsUnpackScaleFactors_MP3_1u8s 
	 ***********************************/
	Ipp8s ScaleFactor[MAX_CHAN * IPP_MP3_SF_BUF_LEN];	/* Table of scalefactors
								 * for 1 or 2 granules, 1 
								 * or 2 channels */
	int Scfsi[MAX_CHAN * SCF_BANDS];	/* Scalefactor select information - 4
						 * scalefactor bands per channel */

	/*******************************
	 4. ippsHuffmanDecode_MP3_1u32s 
	 ********************************/
	Ipp32s IsXr[MAX_CHAN * IPP_MP3_GRANULE_LEN];	/* Buffer used first for Huffman
							 * symbols (Is), then requantized 
							 * IMDCT inputs (Xr) */
	int NonZeroBound[MAX_CHAN];	/* Non-zero bounds on Huffman IMDCT coefficient
					 * set for each channel */

	/*******************************
	 5. ippsReQuantize_MP3_32s_I 
	 ********************************/
	Ipp32s RequantBuf[IPP_MP3_GRANULE_LEN];	/* Work space buffer required by
						 * requantization primitive */

	/*******************************
	 6. ippsMDCTInv_MP3_32s 
	 ********************************/
	Ipp32s Xs[MAX_CHAN * IPP_MP3_GRANULE_LEN];	/* Buffer used for IMDCT
							 * outputs/PQMF synthesis inputs */
	Ipp32s OverlapAddBuf[MAX_CHAN * IPP_MP3_GRANULE_LEN];	/* Overlap-add buffer
								 * required by IMDCT
								 * primitive */
	int PreviousIMDCT[MAX_CHAN];	/* Number of IMDCTs computed on previous
					 * granule/frame */

	/*******************************
	 7. ippsSynthPQMF_MP3_32s16s 
	 ********************************/
	Ipp32s PQMF_V_Buf[MAX_CHAN * IPP_MP3_V_BUF_LEN];	/* "V" buffer - used by
								 * fast DCT-based
								 * algorithm for
								 * synthesis PQMF bank */
	int PQMF_V_Indx[MAX_CHAN];	/* Index used by the PQMF for internal
					 * maintainence of the "V" buffer */

	/*******************************
	 Main Data Decoding Buffer 
	 ********************************/
	Ipp8u MainDataBuf[MAIN_DATA_BUF_SIZE];	/* Decoding buffer for "Main Data"
						 * portion of the MP3 stream. */
	int MainDataEnd;	/* Main Data buffer end pointer */

	/*******************************
	 Application Interface Parameters 
	 ********************************/
	int Channels;		/* Number of channels */
	int pcmLen;		/* Length of pcm output buffer */

} MP3DecoderState;

/*******************************
 MP3 Bit Stream (Ring Buffer) 
********************************/
/*
 * Ring Buffer Notes: 1) Head==Tail signifies buffer empty 2) Len must be radix-2 (i.e.,
 * 4, 8, 16, ..., 8192, 16384, etc.) Decoder uses radix-2 bit mask to force inexpensive
 * wrap-around arithmetic with index, i, updated as follows for reads and writes:
 * (i=i+1)&(Len-1) 
 */
typedef struct {
	Ipp8u Stream[STREAM_BUF_SIZE];	/* Ring buffer */
	unsigned int Head;	/* First element */
	unsigned int Tail;	/* Last element */
	unsigned int Len;	/* Overall buffer size - must be radix-2 */
} MP3BitStream;

/************************
 Function Prototypes 
*************************/
int InitMP3Decoder(MP3DecoderState *, MP3BitStream *);
int DecodeMP3Frame(MP3BitStream *, Ipp16s *, MP3DecoderState *);
