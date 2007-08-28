/*
 * Copyright (C) 2002 Bernard Leach
 *
 * big endian support added 2003 Steven Lucy
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>

#if defined(__ppc__) || defined(__PPC__)
#define IS_BIG_ENDIAN
#endif

typedef struct _image {
	char dev[4];			/* '' */
	unsigned long type;		/* */
	char id[4];			/* 0000 0000 */
	unsigned long devOffset;	/* byte offset of start of image code */
	unsigned long len;		/* length in bytes of image */
	unsigned long addr;		/* load address */
	unsigned long entryOffset;	/* execution start within image */
	unsigned long chksum;		/* checksum for image */
	unsigned long vers;		/* image version */
	unsigned long loadAddr;		/* load address for image */
} image_t;


unsigned long
switch_long(unsigned long l)
{
	return ((l & 0xff) << 24)
		| ((l & 0xff00) << 8)
		| ((l & 0xff0000) >> 8)
		| ((l & 0xff000000) >> 24);
}

void
switch_endian(image_t *image)
{
	image->type = switch_long(image->type);
	image->devOffset = switch_long(image->devOffset);
	image->len = switch_long(image->len);
	image->addr = switch_long(image->addr);
	image->entryOffset = switch_long(image->entryOffset);
	image->chksum = switch_long(image->chksum);
	image->vers = switch_long(image->vers);
	image->loadAddr = switch_long(image->loadAddr);
}

void
print_image(image_t *image)
{
	printf("dev: '%s' type: 0x%4x len: 0x%4x addr: 0x%4x vers: 0x%8x\r\n", image->dev, image->type, image->len, image->addr, image->vers);

	printf("devOffset : 0x%08X entryOffset : 0x%08X loadAddr:0x%08X chksum:0x%08X\n", image->devOffset, image->entryOffset, image->loadAddr, image->chksum);
}

void
usage()
{
	printf("\nUsage: patch_fw ipod_image new_program\n\n");
	printf("Where ipod_image is an image of the iPod firmware\n");
	printf("and new_program is a valid ARM executable\n\n");
	printf("This program is used to patch a program into an image taken\n");
	printf("From an iPod.  The the bootloader table will be updated and\n");
	printf("the new program copied in.\n\n");
}

int
main(int argc, char **argv)
{
	FILE * in;
	image_t image_1, image_2;
	unsigned short fw_version;
 
	if ( argc != 3 ) {
		usage();
		exit(1);
	}

	in = fopen(argv[1], "rb+");
    
	if (in != NULL)
	{
		fseek (in, 0x100 + 10, SEEK_SET);
		fread (&fw_version, sizeof (fw_version), 1, in);

		fseek(in,0x4200,SEEK_SET);
		fread(&image_1, sizeof(image_1), 1, in);
		fread(&image_2, sizeof(image_2), 1, in);
#ifdef IS_BIG_ENDIAN
		switch_endian(&image_1);
		switch_endian(&image_2);
#endif

		FILE *patch;
		unsigned sum = 0, i, len;
		unsigned char temp = 0;
		unsigned long devoff;

		patch = fopen(argv[2], "rb");
		if ( patch != NULL ) {
			fseek(patch, 0x0, SEEK_END);
			len = ftell(patch);
			printf("length for new fw is 0x%x\n", len);

			if ( len > image_2.devOffset - image_1.devOffset) {
				printf("Sorry, new image is too large, image2 offset is 0x%x!   0x%x\n", image_2.devOffset, len);
			}
			else {
				fseek(patch, 0x0, SEEK_SET);
				for ( i = 0; i < len; i++ ) {
					fread(&temp, 1, 1, patch);
					sum = sum + (temp & 0xff);
				}

				printf("Checksum for new fw is 0x%x\n", sum);

				image_1.len = len;
				image_1.chksum = sum;

				/* preserve through endianizing */
				devoff = image_1.devOffset;

				fseek(in,0x4200,SEEK_SET);

#ifdef IS_BIG_ENDIAN
				switch_endian(&image_1);
#endif
				fwrite(&image_1, sizeof(image_1), 1, in);

				/* go to dev offset to write new image */
				fseek(in, devoff + ((fw_version == 3) ? 512 : 0), SEEK_SET);
				/* rewind to start of new image */
				fseek(patch, 0x0, SEEK_SET);
				for ( i = 0; i < len; i++ ) {
					fread(&temp, 1, 1, patch);
					fwrite(&temp, 1, 1, in);
				}
			}

			fclose(patch);
		}
		else {
			printf("Cannot open patch file %s\n", argv[2]);
		}

		fclose(in);
	}
	else {
		printf("Cannot open firmware image file %s\n", argv[1]);
	}

	return 0;
}
