#ifndef _OCIDEC_
#define _OCIDEC_

typedef struct OCIDEC_Status_s
{
    unsigned long IDE_interruptStatus : 1; // bit   0
    unsigned long reserved1           : 5; // bits  5:1
    unsigned long writePingPongFull   : 1; // bit   6
    unsigned long PIO_inProgress      : 1; // bit   7
    unsigned long DMA_reqStatus       : 1; // bit   8
    unsigned long DMA_xmitFull        : 1; // bit   9
    unsigned long DMA_recvEmpty       : 1; // bit  10
    unsigned long reserved2           : 4; // bits 14:11
    unsigned long DMA_inProgress      : 1; // bit  15
    unsigned long reserved3           : 7; // bits 23:16
    unsigned long revisionNr          : 4; // bits 27:24
    unsigned long deviceID            : 4; // bits 31:28
} OCIDEC_Status_t;

// OCIDEC status register bit masks
#define IDE_INTERRUPT_STATUS  0x00000001
#define WRITE_PING_PONG_FULL  0x00000040
#define PIO_IN_PROGRESS       0x00000080
#define DMA_REQ_STATUS        0x00000100
#define DMA_XMIT_FULL         0x00000200
#define DMA_RECV_EMPTY        0x00000400
#define DMA_IN_PROGRESS       0x00008000
#define REVISION_NR           0x0F000000
#define DEVICE_ID             0xF0000000
#define RESERVED_STATUS_BITS  0x00FF713E


typedef struct OCIDEC_Control_s
{
  unsigned long ATA_Reset                     : 1; // bit   0
  unsigned long compatibleTiming_IORDY_enable : 1; // bit   1
  unsigned long fastTimingDev0_IORDY_enable   : 1; // bit   2
  unsigned long fastTimingDev1_IORDY_enable   : 1; // bit   3
  unsigned long writePingPongEnable           : 1; // bit   4
  unsigned long fastTimingDev0Enable          : 1; // bit   5
  unsigned long fastTimingDev1Enable          : 1; // bit   6
  unsigned long IDE_enable                    : 1; // bit   7
  unsigned long endianFlipDev0                : 1; // bit   8
  unsigned long endianFlipDev1                : 1; // bit   9
  unsigned long reserved1                     : 3; // bits 12:10
  unsigned long DMA_dir                       : 1; // bit  13
  unsigned long reserved2                     : 1; // bit  14
  unsigned long DMA_enable                    : 1; // bit  15
  unsigned long reserved3                     : 16;// bits 31:16
} OCIDEC_Control_t;


// OCIDEC Control register bit masks
#define ATA_RESET                       0x00000001
#define COMPATIBLE_TIMING_IORDY_ENABLE  0x00000002
#define FAST_TIMING_DEV0_IORDY_ENABLE   0x00000004
#define FAST_TIMING_DEV1_IORDY_ENABLE   0x00000008
#define WRITE_PING_PONG_ENABLE          0x00000010
#define FAST_TIMING_DEV0_ENABLE         0x00000020
#define FAST_TIMING_DEV1_ENABLE         0x00000040
#define IDE_ENABLE                      0x00000080
#define ENDIAN_FLIP_DEV0                0x00000100
#define ENDIAN_FLIP_DEV1                0x00000200
#define DMA_DIR                         0x00002000
#define DMA_ENABLE                      0x00008000
#define RESERVED_CONTROL_BITS           0xFFFF1C00


typedef volatile struct
{
    unsigned long np_ctrl;  // control register
    unsigned long np_stat;  // status register
    unsigned long np_pctr;  // PIO compatible timing register
    unsigned long np_pftr0; // pio fast timing register device 0
    unsigned long np_pftr1; // pio fast timing register device 1
    unsigned long np_dtr0;  // DMA timing register device 0
    unsigned long np_dtr1;  // DMA timing register device 1
    unsigned long np_dtxdb; // DMA transmit data buffer
} np_OCIDEC;
#define	np_drxdb  np_dtxdb  // DMA receive data buffer


#define DEFAULT_TIMING 0x17021C06

#define MODE0_TIMING   0x07010901
#define MODE1_TIMING   0x01010901
#define MODE2_TIMING   0x01010901
#define MODE3_TIMING   0x01010201
#define MODE4_TIMING   0x01010101

#define MODE0_DTIMING  0x0B010401
#define MODE1_DTIMING  0x06010301
#define MODE2_DTIMING  0x03010201
#define MODE3_DTIMING  0x01010201
#define MODE4_DTIMING  0x01010101


#define VERSION3_REV1  0x31000000
#define OCIDEC_MAX_PIO 4
#define IS_PP_ENABLED  1         /* Using write ping-pong facility, or not. */


#endif

