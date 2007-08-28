#ifndef _SCSI_INQUIRY_H_
#define _SCSI_INQUIRY_H_

#ifdef __cplusplus
extern "C" {
#endif

#define SCSI_INQUIRY_CMD      0x12
#define SCSI_INQUIRY_SBUFSIZE 32
#define SCSI_INQUIRY_DBUFSIZE 255
#define SCSI_INQUIRY_TIMEOUT  2    /* seconds */

#ifdef WIN32
#include <ddk/ntddscsi.h>
typedef struct _SCSI_PASS_THROUGH_WITH_BUFFERS
{
    SCSI_PASS_THROUGH Spt;
    unsigned long filler;
    unsigned char sbuf[32];
    unsigned char dbuf[255];
} SCSI_PASS_THROUGH_WITH_BUFFERS, *PSCSI_PASS_THROUGH_WITH_BUFFERS;
#endif

int scsi_inquiry_get_data (int dev, unsigned char *buf, int buf_size);
int scsi_inquiry_get_hw_ver (int dev);

#ifdef __cplusplus
}
#endif

#endif
