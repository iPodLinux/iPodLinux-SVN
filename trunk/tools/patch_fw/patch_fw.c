/*
 * Copyright (C) 2002 Bernard Leach
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

#define VERSION 0x120

typedef struct _image {
	char type[4];			/* '' */
	unsigned long id;		/* */
	char pad1[4];			/* 0000 0000 */
	unsigned long devOffset;	/* byte offset of start of image code */
	unsigned long len;		/* length in bytes of image */
	unsigned long addr;		/* load address */
	unsigned long entryOffset;	/* execution start within image */
	unsigned long chksum;		/* checksum for image */
	unsigned long vers;		/* image version */
	unsigned long loadAddr;		/* load address for image */
} image_t;

void
print_image(image_t *image)
{
	printf("type: '%s' id: 0x%4x len: 0x%4x addr: 0x%4x vers: 0x%8x\r\n", image->type, image->id, image->len, image->addr, image->vers);

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
	printf("Only version %x.%x of the firmware can be patched\n\n", ((VERSION>>8)&0xff), (VERSION&0xff));
}

int
main(int argc, char **argv)
{
	FILE * in;
	image_t image_1, image_2;
 
	if ( argc != 3 ) {
		usage();
		exit(1);
	}

	in = fopen(argv[1], "r+");
    
	if (in != NULL)
	{
		fseek(in,0x4200,SEEK_SET);
		fread(&image_1, sizeof(image_1), 1, in);
		fread(&image_2, sizeof(image_2), 1, in);

		if ( image_1.vers != VERSION ) {
			printf("Can only patch version 0x%0x, fw is 0x%0x\n", VERSION, image_1.vers);
		}
		else {
			FILE *patch;
			unsigned sum = 0, temp = 0, i, len;

			patch = fopen(argv[2], "r");
			if ( patch != NULL ) {
				fseek(patch, 0x0, SEEK_END);
				len = ftell(patch);
				printf("length for new fw is 0x%x\n", len);

				if ( len > image_2.devOffset ) {
					printf("Sorry, new image is too large, image2 offset is 0x%x!\n", image_2.devOffset);
					
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

					fseek(in,0x4200,SEEK_SET);
					fwrite(&image_1, sizeof(image_1), 1, in);

					/* go to dev offset to write new image */
					fseek(in, image_1.devOffset, SEEK_SET);
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
		}

		fclose(in);
	}
	else {
		printf("Cannot open firmware image file %s\n", argv[1]);
	}

	return 0;
}
