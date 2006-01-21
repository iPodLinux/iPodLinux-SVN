#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include "hotdog.h"
#include "hotdog_font.h"

#define HDF_MAGIC 0x676f6448

/* The HDF header. All fields are stored big-endian. */

typedef struct HDF_header
{
    uint32  magic;              /* 'Hdog' = 0x676f6448 */
    uint16  nchars;             /* number of characters in the font */
    uint16  firstchar;          /* first character */
    uint8   bpp;                /* bits per pixel: 1, 8, or 32 */
    uint8   bitstype;           /* HD_FONT_PITCHED or HD_FONT_CLUMPED */
    uint16  height;             /* height of the font, height of the data surface */
    uint32  pitch;              /* width in bytes of a scanline in the data surface.
                                   Only valid for PITCHED fonts. */
    uint32  nbytes;             /* Number of bytes of character data to read */
} __attribute__ ((packed)) HDF_header;

/* Following the header are <nchars> 32-bit BE values indicating the offset
 * in pixels into the data surface.
 * (The first one is char <firstchar>, second is char <firstchar>+1, etc.)
 *
 * Following that are <nchars> bytes indicating the *width* of
 * each character, indexed in the same way.
 *
 * Following that is the data surface.
 */

/* 1-bpp fonts look like
 *
 * MSb, byte 0          LSb  MSb, byte 1          LSb
 *  |                    |    |                    |
 *  v                    v    v                    v
 *  0  1  2  3  4  5  6  7    8  9 10 11 12 13 14 15    ...
 *
 * That is, the bits are arranged in bytes, MSb first. The numbers
 * indicate bit positions (0=first bit, 1=second, ...).
 */

/* 32-bit fonts are stored in AA RR GG BB byte order (ARGB big-endian),
 * and cannot be colored differently from their color in the file. 
 */

hd_font *HD_Font_LoadHDF (const char *filename) 
{
    int i;
    FILE *fp = fopen (filename, "rb");
    if (!fp) return 0;
    
    HDF_header hdr;
    fread (&hdr, 20, 1, fp);

    hdr.magic     = ntohl (hdr.magic);
    hdr.nchars    = ntohs (hdr.nchars);
    hdr.firstchar = ntohs (hdr.firstchar);
    hdr.height    = ntohs (hdr.height);
    hdr.pitch     = ntohl (hdr.pitch);
    hdr.nbytes    = ntohl (hdr.nbytes);

    if (hdr.magic != HDF_MAGIC || (hdr.bpp != 1 && hdr.bpp != 8 && hdr.bpp == 32)) {
        fclose (fp);
        return 0;
    }

    hd_font *font = malloc (sizeof(hd_font));
    font->bitstype    = hdr.bitstype;
    font->pitch       = hdr.pitch;
    font->h           = hdr.height;
    font->bpp         = hdr.bpp;
    font->firstchar   = hdr.firstchar;
    font->nchars      = hdr.nchars;
    font->defaultchar = 0;
    font->pixbytes    = hdr.nbytes;
    font->offset      = malloc (4 * font->nchars);
    font->width       = malloc (font->nchars);
    font->pixels      = malloc (font->pixbytes);
    
    if (!font->offset || !font->width || !font->pixels) {
        if (font->offset) free (font->offset);
        if (font->width)  free (font->width);
        if (font->pixels) free (font->pixels);
        free (font);
        return 0;
    }

    fread (font->offset, 4, font->nchars, fp);
    for (i = 0; i < font->nchars; i++)
        font->offset[i] = ntohl (font->offset[i]);

    fread (font->width, 1, font->nchars, fp);

    fread (font->pixels, font->pixbytes, 1, fp);
    if (font->bpp == 32) {
        uint32 *pp;
        for (pp = font->pixels; pp < (uint32 *)((char *)font->pixels + font->pixbytes); pp++)
            *pp = ntohl (*pp);
    }

    return font;
}

int HD_Font_SaveHDF (hd_font *font, const char *filename) 
{
    int i;
    FILE *fp = fopen (filename, "wb");
    if (!fp) return -1;
    
    HDF_header hdr;
    hdr.magic     = htonl (HDF_MAGIC);
    hdr.nchars    = htons (font->nchars);
    hdr.firstchar = htons (font->firstchar);
    hdr.bpp       = font->bpp;
    hdr.bitstype  = font->bitstype;
    hdr.height    = htons (font->h);
    hdr.pitch     = htonl (font->pitch);
    hdr.nbytes    = htonl (font->pixbytes);

    fwrite (&hdr, 16, 1, fp);

    for (i = 0; i < font->nchars; i++)
        font->offset[i] = htonl (font->offset[i]);

    fwrite (font->offset, 4, font->nchars, fp);

    for (i = 0; i < font->nchars; i++)
        font->offset[i] = ntohl (font->offset[i]);
    
    fwrite (font->width, 1, font->nchars, fp);
    
    if (font->bpp == 32) {
        uint32 *pp;
        for (pp = font->pixels; pp < (uint32 *)((char *)font->pixels + font->pixbytes); pp++)
            *pp = htonl (*pp);
    }

    fwrite (font->pixels, font->pixbytes, 1, fp);
    
    if (font->bpp == 32) {
        uint32 *pp;
        for (pp = font->pixels; pp < (uint32 *)((char *)font->pixels + font->pixbytes); pp++)
            *pp = ntohl (*pp);
    }

    fclose (fp);

    return 0;
}


/* Utility functions for LoadFNT and LoadPCF - read big-endian datums. */
static int read16 (FILE *fp, uint16 *sp)
{
    int c;
    unsigned short s = 0;
    if ((c = getc (fp)) == EOF) return 0;
    s |= (c & 0xff);
    if ((c = getc (fp)) == EOF) return 0;
    s |= (c & 0xff) << 8;
    *sp = s;
    return 1;
}
static int read32 (FILE *fp, uint32 *lp)
{
    int c;
    unsigned long l = 0;
    if ((c = getc (fp)) == EOF) return 0;
    l |= (c & 0xff);
    if ((c = getc (fp)) == EOF) return 0;
    l |= (c & 0xff) << 8;
    if ((c = getc (fp)) == EOF) return 0;
    l |= (c & 0xff) << 16;
    if ((c = getc (fp)) == EOF) return 0;
    l |= (c & 0xff) << 24;
    *lp = l;
    return 1;
}
static int readstr (FILE *fp, char *buf, int count)
{
    return fread (buf, 1, count, fp);
}

hd_font *HD_Font_LoadFNT (const char *filename) 
{
    int i;
    FILE *fp = fopen (filename, "rb");
    if (!fp) return 0;
    
    uint16 maxwidth, height, ascent, pad;
    uint32 firstchar, defaultchar, nchars;
    uint32 nbits, noffset, nwidth;
    char version[5];
    
    memset (version, 0, 5);
    if (readstr (fp, version, 4) != 4)
        goto errout;
    if (strcmp (version, "RB11") != 0)
        goto errout;
    
    fseek (fp, 0x140, SEEK_CUR); // skip name and copyright

    if (!read16 (fp, &maxwidth)) goto errout;
    if (!read16 (fp, &height)) goto errout;
    if (!read16 (fp, &ascent)) goto errout;
    if (!read16 (fp, &pad)) goto errout;
    if (!read32 (fp, &firstchar)) goto errout;
    if (!read32 (fp, &defaultchar)) goto errout;
    if (!read32 (fp, &nchars)) goto errout;
    if (!read32 (fp, &nbits)) goto errout; // nbits = number of *words* of bmap data
    if (!read32 (fp, &noffset)) goto errout;
    if (!read32 (fp, &nwidth)) goto errout;
    
    hd_font *font     = malloc (sizeof(hd_font));
    font->bitstype    = HD_FONT_CLUMPED;
    font->pitch       = 2;
    font->h           = height;
    font->bpp         = 1;
    font->firstchar   = firstchar;
    font->nchars      = nchars;
    font->defaultchar = defaultchar;
    font->pixels      = malloc (font->pixbytes = 2 * nbits);
    font->offset      = malloc (4 * nchars);
    font->width       = malloc (nchars);

    if (!font->pixels || !font->offset || !font->width) {
        goto errfree;
    }

    if (nbits & 1) nbits++;
    fread (font->pixels, 2, nbits, fp);

    if (!noffset) goto errfree;

    if (noffset > nchars) noffset = nchars;
    for (i = 0; i < noffset; i++) {
        if (!read32 (fp, font->offset + i))
            goto errfree;
    }
    if (noffset < nchars) printf ("warning: too few offset data\n");

    if (nwidth) {
        if (nwidth > nchars) nwidth = nchars;
        fread (font->width, 1, nwidth, fp);
        if (nwidth < nchars) memset (font->width + nwidth, maxwidth, nchars - nwidth);
    } else {
        memset (font->width, maxwidth, nchars);
    }

    fclose (fp);
    return font;
    
 errfree:
    if (font->offset) free (font->offset);
    if (font->width) free (font->width);
    if (font->pixels) free (font->pixels);
    free (font);
 errout:
    fclose (fp);
    return 0;
}
