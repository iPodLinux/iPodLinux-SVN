/*
 *
 *  Copyright (c) 2007 Felix Bruns
 *  Parts Copyright (c) 1999 Douglas Gilbert
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef WIN32
  #include <tchar.h>
  #include <stddef.h>
  #include <windows.h>
#else
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
  #include <scsi/sg.h>
  #include <sys/ioctl.h>
#endif
    
#include "scsi_inquiry.h"

int scsi_inquiry_get_data (int dev, unsigned char *buf, int buf_size)
{
#ifdef WIN32
    HANDLE *fd;
    SCSI_PASS_THROUGH_WITH_BUFFERS sptwb;
    unsigned long returned, length;
    wchar_t device[] = _TEXT("\\\\.\\PhysicalDriveX");
    device[17] = dev + '0';
#else
    int fd;
    sg_io_hdr_t io_hdr;
    unsigned char cdb[6] = {SCSI_INQUIRY_CMD, 1, 0xC0, 0, SCSI_INQUIRY_DBUFSIZE, 0};
    unsigned char sbuf[SCSI_INQUIRY_SBUFSIZE];
    unsigned char dbuf[SCSI_INQUIRY_DBUFSIZE];
    char device[] = "/dev/sdX";
    device[7] = dev + 'a';
#endif
    
    int page_code, page_start, page_end, status, size = 0;
    
#ifdef WIN32
    fd = CreateFile(device, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
#else
    fd = open(device, O_RDONLY);
#endif
#ifdef WIN32
    if (fd == INVALID_HANDLE_VALUE) {
#else
    if (fd < 0) {
#endif
        fprintf(stderr, "Error opening device.\n");
        return 1;
    }
    else {
#ifdef WIN32
        memset(&sptwb, 0, sizeof(SCSI_PASS_THROUGH_WITH_BUFFERS));
        sptwb.Spt.Length             = sizeof(SCSI_PASS_THROUGH);
        sptwb.Spt.PathId             = 0;
        sptwb.Spt.TargetId           = 1;
        sptwb.Spt.Lun                = 0;
        sptwb.Spt.CdbLength          = 6;
        sptwb.Spt.SenseInfoLength    = SCSI_INQUIRY_SBUFSIZE;
        sptwb.Spt.DataIn             = SCSI_IOCTL_DATA_IN;
        sptwb.Spt.DataTransferLength = SCSI_INQUIRY_DBUFSIZE;
        sptwb.Spt.TimeOutValue       = SCSI_INQUIRY_TIMEOUT;
        sptwb.Spt.DataBufferOffset   = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS, dbuf);
        sptwb.Spt.SenseInfoOffset    = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS, sbuf);
        sptwb.Spt.Cdb[0]             = SCSI_INQUIRY_CMD;
        sptwb.Spt.Cdb[1]            |= 1;
        sptwb.Spt.Cdb[2]             = 0xC0;
        sptwb.Spt.Cdb[4]             = SCSI_INQUIRY_DBUFSIZE;
        
        length = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS, dbuf) + sptwb.Spt.DataTransferLength;
        status = DeviceIoControl(fd, IOCTL_SCSI_PASS_THROUGH, &sptwb, sizeof(SCSI_PASS_THROUGH), &sptwb, length, &returned, NULL);
#else
        memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
        io_hdr.interface_id = 'S';
        io_hdr.cmd_len = sizeof(cdb);
        io_hdr.mx_sb_len = sizeof(sbuf);
        io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
        io_hdr.dxfer_len = SCSI_INQUIRY_DBUFSIZE;
        io_hdr.dxferp = dbuf;
        io_hdr.cmdp = cdb;
        io_hdr.sbp = sbuf;
        io_hdr.timeout = SCSI_INQUIRY_TIMEOUT * 1000;
        
        status = ioctl(fd, SG_IO, &io_hdr);
#endif
        
        if (status < 0) {
            fprintf(stderr, "Error performing SCSI INQUIRY command.\n");
            return 1;
        }
        else {
#ifdef WIN32
            if (sptwb.dbuf[1] != 0xC0) {
                fprintf(stderr, "Device didn't send expected data.\n");
                return 1;
            }
            
            page_start = sptwb.dbuf[4];
            page_end   = sptwb.dbuf[3+sptwb.dbuf[3]];
#else
            if (dbuf[1] != 0xC0) {
                fprintf(stderr, "Device didn't send expected data.\n");
                return 1;
            }
            
            page_start = dbuf[4];
            page_end   = dbuf[3+dbuf[3]];
#endif
            
            for (page_code = page_start; page_code <= page_end; page_code++) {
#ifdef WIN32
                memset(&sptwb, 0, sizeof(SCSI_PASS_THROUGH_WITH_BUFFERS));
                sptwb.Spt.Length             = sizeof(SCSI_PASS_THROUGH);
                sptwb.Spt.PathId             = 0;
                sptwb.Spt.TargetId           = 1;
                sptwb.Spt.Lun                = 0;
                sptwb.Spt.CdbLength          = 6;
                sptwb.Spt.SenseInfoLength    = SCSI_INQUIRY_SBUFSIZE;
                sptwb.Spt.DataIn             = SCSI_IOCTL_DATA_IN;
                sptwb.Spt.DataTransferLength = SCSI_INQUIRY_DBUFSIZE;
                sptwb.Spt.TimeOutValue       = SCSI_INQUIRY_TIMEOUT;
                sptwb.Spt.DataBufferOffset   = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS, dbuf);
                sptwb.Spt.SenseInfoOffset    = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS, sbuf);
                sptwb.Spt.Cdb[0]             = SCSI_INQUIRY_CMD;
                sptwb.Spt.Cdb[1]            |= 1;
                sptwb.Spt.Cdb[2]             = page_code;
                sptwb.Spt.Cdb[4]             = SCSI_INQUIRY_DBUFSIZE;
                
                length = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS, dbuf) + sptwb.Spt.DataTransferLength;
                status = DeviceIoControl(fd, IOCTL_SCSI_PASS_THROUGH, &sptwb, sizeof(SCSI_PASS_THROUGH), &sptwb, length, &returned, NULL);
#else
                io_hdr.cmdp[2] = page_code;
                
                status = ioctl(fd, SG_IO, &io_hdr);
#endif
                
#ifdef WIN32
                if (!status) {
                    fprintf(stderr, "Error performing SCSI INQUIRY command.\n");
                    return 1;
                }
                else {
                    if (size+sptwb.dbuf[3] > buf_size) return 1;
                    memcpy(buf+size, sptwb.dbuf+4, sptwb.dbuf[3]);
                    size += sptwb.dbuf[3];
                }
#else
                if (status < 0) {
                    fprintf(stderr, "Error performing SCSI INQUIRY command.\n");
                    return 1;
                }
                else {
                    if (size+dbuf[3] > buf_size) return 1;
                    memcpy(buf+size, dbuf+4, dbuf[3]);
                    size += dbuf[3];
                }
#endif
            }
        }
        
        buf[size] = '\0';
        
#ifdef WIN32
        CloseHandle(fd);
#else
        close(fd);
#endif
    }
    
    return 0;
}

int scsi_inquiry_get_hw_ver (int dev)
{
    unsigned char buffer[10240], *ptr;
    int err, ufid, hw_ver;
    
    err = scsi_inquiry_get_data(dev, buffer, 10240);
    
    if (err) {
        hw_ver = -1;
    }
    else {
        ptr = (unsigned char *)strstr((char *)buffer, "UpdaterFamilyID");
        while (!isdigit(*ptr)) ptr++;
        ufid = atoi((char *)ptr);
        
        int ufid_hw_ver[] = { 0x0, // Unknown
                              0x1, // 1G/2G */*** (0x1 or 0x2)
                              0x3, // 3G *
                              0x4, // Mini 1G *
                              0x5, // 4G
                              0x6, // 4G Photo/Color
                              0x4, // Mini 1G *
                              0x7, // Mini 2G
                              0x0, // Unknown
                              0x0, // Unknown
                              0x5, // 4G
                              0x6, // 4G Photo/Color
                              0x0, // Unknown
                              0xB, // 5G
                              0xC, // Nano 1G
                              0x0, // Unknown
                              0x0, // Unknown
                              0xC, // Nano 1G
                              0x0, // Unknown
                              0x0, // Nano 2G **
                              0xB, // 5G
                              0x0, // Unknown
                              0x0, // Unknown
                              0x0, // Unknown
                              0x0, // Unknown
                              0x0, // 5.5G **
                              };
                                
        // *   (Shouldn't respond to SCSI INQUIRY)
        // **  (Not supported)
        // *** (UpdaterFamilyID is same for 1G and 2G)
                                
        hw_ver = ufid_hw_ver[ufid];
    }
    
    return hw_ver;
}
