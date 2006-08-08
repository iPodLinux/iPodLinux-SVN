#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>

#include "hotdog.h"

#define HDF_MAGIC 0x676f6448

/************ Drawing and objecting ****************/
static void _do_draw_char8 (hd_surface srf, hd_font *font, int x, int y, uint32 color, uint16 ch, int cblend)
{
    uint8 *pixels = (uint8 *)font->pixels + font->offset[ch];
    int width = font->width[ch];
    int sx = x, ex = x + width, ey = y + font->h;
    while (y < ey) {
        uint32 pix;
        x = sx;
        while (x < ex) {
            pix = HD_MASKPIX (color, *pixels++);
            if (cblend)
                HD_SRF_BLENDPIX (srf, x, y, pix);
            else
                HD_SRF_SETPIX (srf, x, y, pix);
            x++;
        }
        if (font->bitstype == HD_FONT_CLUMPED)
            pixels += font->pitch - (width & (font->pitch - 1));
        else
            pixels += font->pitch - width;
        y++;
    }
}
static void _do_draw_char8_fast (hd_surface srf, hd_font *font, int x, int y, uint32 color, uint16 ch, int cblend)
{
    uint8 *pixels = (uint8 *)font->pixels + font->offset[ch];
    uint32 *dest = HD_SRF_PIXPTR (srf, x, y);
    uint32 pix;
    int width = font->width[ch];
    int sx = x, ex = x + width, ey = y + font->h;

    color = ~color;
    while (y < ey) {
        x = sx;
        while (x < ex) {
            pix = *pixels++;
            pix |= (pix << 8);
            pix |= (pix << 16);
            *dest++ = pix ^ color;
            x++;
        }
        if (font->bitstype == HD_FONT_CLUMPED)
            pixels += font->pitch - (width & (font->pitch - 1));
        else
            pixels += font->pitch - width;
        dest += HD_SRF_WIDTH (srf) - width;
        y++;
    }
}
static void _do_draw_char32 (hd_surface srf, hd_font *font, int x, int y, uint32 color, uint16 ch, int cblend)
{
    uint32 *pixels = (uint32 *)font->pixels + font->offset[ch];
    int width = font->width[ch];
    int sx = x, ex = x + width, ey = y + font->h;
    while (y < ey) {
        x = sx;
        while (x < ex) {
            HD_SRF_BLENDPIX (srf, x, y, *pixels++);
            x++;
        }
        if (font->bitstype == HD_FONT_PITCHED)
            pixels += font->pitch - width;
        y++;
    }
}
static void _do_draw_char32_fast (hd_surface srf, hd_font *font, int x, int y, uint32 color, uint16 ch, int cblend)
{
    uint32 *pixels = (uint32 *)font->pixels + font->offset[ch];
    uint32 *dest = HD_SRF_PIXPTR (srf, x, y);
    int width = font->width[ch];
    int ey = y + font->h;
    while (y < ey) {
        memcpy (dest, pixels, 4*width);
        if (font->bitstype == HD_FONT_CLUMPED)
            pixels += width;
        else
            pixels += (font->pitch >> 2);
        dest += HD_SRF_WIDTH (srf);
        y++;
    }
}
static void _do_draw_char1 (hd_surface srf, hd_font *font, int x, int y, uint32 color, uint16 ch, int cblend) 
{
    uint8 *pixels = (uint8 *)font->pixels + font->offset[ch];
    int ofs = 0;
    int width = font->width[ch];
    int sx = x, ex = x + width, ey = y + font->h;

    while (y < ey) {
        int byte = 0;
        x = sx;
        ofs = 0;
        
        // Read this row.
        while (x < ex) {
            if (!(ofs & 7)) byte = *pixels++;
            if (byte & 0x80) {
                if (cblend)
                    HD_SRF_BLENDPIX (srf, x, y, color);
                else
                    HD_SRF_SETPIX (srf, x, y, color);
            }
            byte <<= 1;
            ofs++;
            x++;
        }
        
        ofs = (ofs + 7) >> 3; // ofs = number of bytes consumed
        if (font->bitstype == HD_FONT_CLUMPED) {
            // Align to multiple of pitch.
            while (ofs & (font->pitch - 1)) {
                ofs++;
                pixels++;
            }
        } else {
            // Advance to next row.
            pixels += font->pitch - ofs;
        }
        y++;
    }
}

typedef void draw_func (hd_surface srf, hd_font *font, int x, int y, uint32 color, uint16 ch, int blend_all);

static inline draw_func *_do_pick_draw_func (int bpp, int blend_parts) 
{
    switch (bpp + !blend_parts) {
    case 1:
    case 2:
        return _do_draw_char1;
    case 8:
        return _do_draw_char8;
    case 9:
        return _do_draw_char8_fast;
    case 32:
        return _do_draw_char32;
    case 33:
        return _do_draw_char32_fast;
    default:
        fprintf (stderr, "Invalid bpp in font!\n");
        abort();
    }
    return 0;
}

static void _do_draw_lat8 (hd_surface srf, hd_font *font, int x, int y, uint32 color, const char *str,
                           int blend_all, int blend_parts) 
{
    int cx = x, cy = y;
    const char *p = str;
    draw_func *func = _do_pick_draw_func (font->bpp, blend_parts);

    while (*p) {
        if (*p == '\n') {
            cx = x;
            cy += font->h + 1;
        }
        else if (*p == ' ') {
            int ch = 0x20 - font->firstchar; 
            cx += font->width[ch < 0 ? 0 : ch];
        }
        else if (*p > font->firstchar && *p < font->firstchar + font->nchars) {
            (*func)(srf, font, cx, cy, color, *p - font->firstchar, blend_all);
            cx += font->width[*p - font->firstchar];
        }
        p++;
    }
}

static void _do_draw_uc16 (hd_surface srf, hd_font *font, int x, int y, uint32 color, const uint16 *str,
                           int blend_all, int blend_parts)
{
    int cx = x, cy = y;
    const uint16 *p = str;
    draw_func *func = _do_pick_draw_func (font->bpp, blend_parts);

    while (*p) {
        if (*p == '\n') {
            cx = x;
            cy += font->h + 1;
        }
        else if (*p > font->firstchar && *p < font->firstchar + font->nchars) {
            (*func)(srf, font, cx, cy, color, *p - font->firstchar, blend_all);
            cx += font->width[*p - font->firstchar];
        }
        p++;
    }
}

void HD_Font_DrawLatin1 (hd_surface srf, hd_font *font, int x, int y, uint32 color, const char *str) 
{
    _do_draw_lat8 (srf, font, x, y, color, str, (color & 0xff000000) != 0xff000000, 1);
}
void HD_Font_DrawFastLatin1 (hd_surface srf, hd_font *font, int x, int y, uint32 color, const char *str) 
{
    int luminance;
    luminance = (((color & 0x00FF0000) >> (16 + 2)) +
                 ((color & 0x0000FF00) >> ( 8 + 1)) +
                 ((color & 0x0000FF00) >> ( 8 + 3)) +
                 ((color & 0x000000FF) >> ( 0 + 3)));
    _do_draw_lat8 (srf, font, x, y, (font->bpp == 1)? color : (luminance > 128)? 0xffffffff : 0, str, 0, 0);
}
void HD_Font_DrawUnicode (hd_surface srf, hd_font *font, int x, int y, uint32 color, const uint16 *str) 
{
    _do_draw_uc16 (srf, font, x, y, color, str, (color & 0xff000000) != 0xff000000, 1);
}
void HD_Font_DrawFastUnicode (hd_surface srf, hd_font *font, int x, int y, uint32 color, const uint16 *str) 
{
    int luminance;
    luminance = (((color & 0x00FF0000) >> (16 + 2)) +
                 ((color & 0x0000FF00) >> ( 8 + 1)) +
                 ((color & 0x0000FF00) >> ( 8 + 3)) +
                 ((color & 0x000000FF) >> ( 0 + 3)));
    _do_draw_uc16 (srf, font, x, y, (font->bpp == 1)? color : (luminance > 128)? 0xffffffff : 0, str, 0, 0);
}

static int IsASCII (const char *str) 
{
    const char *p = str;
    while (*p) {
        if (*p & 0x80) return 0;
        p++;
    }
    return 1;
}

static int ConvertUTF8 (const unsigned char *src, unsigned short *dst)
{
    const unsigned char *sp = src;
    unsigned short *dp = dst;
    int len = 0;
    while (*sp) {
        *dp = 0;
        if (*sp < 0x80) *dp = *sp++;
        else if (*sp >= 0xC0 && *sp < 0xE0) {
            *dp |= (*sp++ - 0xC0) << 6;  if (!*sp) goto err;
            *dp |= (*sp++ - 0x80);
        }
        else if (*sp >= 0xE0 && *sp < 0xF0) {
            *dp |= (*sp++ - 0xE0) << 12; if (!*sp) goto err;
            *dp |= (*sp++ - 0x80) << 6;  if (!*sp) goto err;
            *dp |= (*sp++ - 0x80);
        }
        else if (*sp >= 0xF0 && *sp < 0xF8) {
            *dp |= (*sp++ - 0xF0) << 18; if (!*sp) goto err;
            *dp |= (*sp++ - 0x80) << 12; if (!*sp) goto err;
            *dp |= (*sp++ - 0x80) << 6;  if (!*sp) goto err;
            *dp |= (*sp++ - 0x80);
        }
        else if (*sp >= 0xF8 && *sp < 0xFC) {
            *dp |= (*sp++ - 0xF8) << 24; if (!*sp) goto err;
            *dp |= (*sp++ - 0x80) << 18; if (!*sp) goto err;
            *dp |= (*sp++ - 0x80) << 12; if (!*sp) goto err;
            *dp |= (*sp++ - 0x80) << 6;  if (!*sp) goto err;
            *dp |= (*sp++ - 0x80);
        }
        else if (*sp >= 0xFC && *sp < 0xFE) {
            *dp |= (*sp++ - 0xF8) << 30; if (!*sp) goto err;
            *dp |= (*sp++ - 0x80) << 24; if (!*sp) goto err;
            *dp |= (*sp++ - 0x80) << 18; if (!*sp) goto err;
            *dp |= (*sp++ - 0x80) << 12; if (!*sp) goto err;
            *dp |= (*sp++ - 0x80) << 6;  if (!*sp) goto err;
            *dp |= (*sp++ - 0x80);
        }
        else goto err;
        
        dp++;
        len++;
        continue;
        
    err:
        *dp++ = '?';
        sp++;
        len++;
    }
    *dp = 0;
    return len;
}

void HD_Font_Draw (hd_surface srf, hd_font *font, int x, int y, uint32 color, const char *str) 
{
    if (IsASCII (str))
        HD_Font_DrawLatin1 (srf, font, x, y, color, str);
    else {
        unsigned short *buf = malloc (strlen (str) * 2 + 2);
        int len = ConvertUTF8 (str, buf);
        buf[len] = 0;
        HD_Font_DrawUnicode (srf, font, x, y, color, buf);
        free (buf);
    }
}

void HD_Font_DrawFast (hd_surface srf, hd_font *font, int x, int y, uint32 color, const char *str) 
{
    if (IsASCII (str))
        HD_Font_DrawFastLatin1 (srf, font, x, y, color, str);
    else {
        unsigned short *buf = malloc (strlen (str) * 2 + 2);
        int len = ConvertUTF8 (str, buf);
        buf[len] = 0;
        HD_Font_DrawFastUnicode (srf, font, x, y, color, buf);
        free (buf);
    }
}

int HD_Font_TextWidthLatin1 (hd_font *font, const char *str) 
{
    const char *p = str;
    int ret = 0;
    
    if (!font || !str || !font->width) return -1;

    while (*p) {
        if (*p - font->firstchar <= font->nchars)
            ret += font->width[*p - font->firstchar];
        p++;
    }

    return ret;
}

int HD_Font_TextWidthUnicode (hd_font *font, const uint16 *str) 
{
    const uint16 *p = str;
    int ret = 0;
    
    if (!font || !str || !font->width) return -1;

    while (*p) {
        if (*p - font->firstchar <= font->nchars)
            ret += font->width[*p - font->firstchar];
        p++;
    }

    return ret;
}

int HD_Font_TextWidth (hd_font *font, const char *str) 
{
    if (IsASCII (str))
        return HD_Font_TextWidthLatin1 (font, str);
    else {
        unsigned short *buf = malloc (strlen (str) * 2 + 2);
        int len = ConvertUTF8 (str, buf);
        buf[len] = 0;
        int ret = HD_Font_TextWidthUnicode (font, buf);
        free (buf);
        return ret;
    }
}

hd_object *HD_Font_MakeObject (hd_font *font, uint32 col, const char *str) 
{
    hd_object *ret = HD_Canvas_Create (HD_Font_TextWidth (font, str), font->h);
    if (!ret) return 0;
    
    HD_Font_Draw (ret->canvas, font, 0, 0, col, str);
    return ret;
}

/*********************** HDF ***********************/

/* The HDF header. All fields are stored big-endian. */

typedef struct HDF_header
{
    uint32  magic;              /* 'Hdog' = 0x676f6448 */
    uint16  nchars;             /* number of characters in the font */
    uint16  firstchar;          /* first character */
    uint8   bpp;                /* bits per pixel: 1, 8, or 32 */
    uint8   bitstype;           /* HD_FONT_PITCHED or HD_FONT_CLUMPED */
    uint16  height;             /* height of the font, height of the data surface */
    uint32  pitch;              /* width in bytes of a scanline in the data surface, for PITCHED fonts.
                                   Has a different meaning for CLUMPED fonts; see hotdog.h. */
    uint32  nbytes;             /* Number of bytes of character data to read */
    /* 32-bit AARRGGBB bytes are premultiplied! */
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
 * The ARGB bytes are premultiplied.
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


/*********************** FNT ***********************/
/** Most of this code is taken from Microwindows. **/

/* Utility functions for LoadFNT and LoadPCF - read little-endian datums. */
static int read16 (FILE *fp, uint16 *sp)
{
    int c;
    uint16 s = 0;
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
    uint32 l = 0;
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
    uint8 *pix = font->pixels;
    for (i = 0; i < nbits; i++) {
        uint8 tmp;
        tmp = pix[2*i+1];
        pix[2*i+1] = pix[2*i];
        pix[2*i] = tmp;
    }

    if (!noffset) goto errfree;

    if (noffset > nchars) noffset = nchars;
    for (i = 0; i < noffset; i++) {
        if (!read32 (fp, font->offset + i))
            goto errfree;
        font->offset[i] *= 2;
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


/*********************** PCF ***********************/
/** Most of this code is taken from Microwindows. **/

/* These are maintained statically for ease FIXME*/
static struct toc_entry *toc;
static uint32 toc_size;

/* Various definitions from the Free86 PCF code */
#define PCF_FILE_VERSION	(('p'<<24)|('c'<<16)|('f'<<8)|1)
#define PCF_PROPERTIES		(1 << 0)
#define PCF_ACCELERATORS	(1 << 1)
#define PCF_METRICS		(1 << 2)
#define PCF_BITMAPS		(1 << 3)
#define PCF_INK_METRICS		(1 << 4)
#define PCF_BDF_ENCODINGS	(1 << 5)
#define PCF_SWIDTHS		(1 << 6)
#define PCF_GLYPH_NAMES		(1 << 7)
#define PCF_BDF_ACCELERATORS	(1 << 8)
#define PCF_FORMAT_MASK		0xFFFFFF00
#define PCF_DEFAULT_FORMAT	0x00000000

#define PCF_GLYPH_PAD_MASK	(3<<0)
#define PCF_BYTE_MASK		(1<<2)
#define PCF_BIT_MASK		(1<<3)
#define PCF_SCAN_UNIT_MASK	(3<<4)
#define GLYPHPADOPTIONS		4

#define PCF_LSB_FIRST		0
#define PCF_MSB_FIRST		1

#ifndef _BIG_ENDIAN

/* little endian - no action required */
# define wswap(x)       (x)
# define dwswap(x)      (x)

#else
/** Convert little-endian 16-bit number to the host CPU format. */
# define wswap(x)       ((((x) << 8) & 0xff00) | (((x) >> 8) & 0x00ff))
/** Convert little-endian 32-bit number to the host CPU format. */
# define dwswap(x)      ((((x) << 24) & 0xff000000L) | \
                         (((x) <<  8) & 0x00ff0000L) | \
                         (((x) >>  8) & 0x0000ff00L) | \
                         (((x) >> 24) & 0x000000ffL) )
#endif

/* A few structures that define the various fields within the file */
struct toc_entry {
	int type;
	int format;
	int size;
	int offset;
};

struct prop_entry {
	unsigned int name;
	unsigned char is_string;
	unsigned int value;
};

struct string_table {
	unsigned char *name;
	unsigned char *value;
};

struct metric_entry {
	short leftBearing;
	short rightBearing;
	short width;
	short ascent;
	short descent;
	short attributes;
};

struct encoding_entry {
	uint16 min_byte2;	/* min_char or min_byte2 */
	uint16 max_byte2;	/* max_char or max_byte2 */
	uint16 min_byte1;	/* min_byte1 (hi order) */
	uint16 max_byte1;	/* max_byte1 (hi order) */
	uint16 defaultchar;
	uint32 count;		/* count of map entries */
	uint16 *map;		/* font index -> glyph index */
};

/* This is used to quickly reverse the bits in a field */
static unsigned char _reverse_byte[0x100] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

/*
 *	Invert bit order within each BYTE of an array.
 */
static void
bit_order_invert(unsigned char *buf, int nbytes)
{
	for (; --nbytes >= 0; buf++)
		*buf = _reverse_byte[*buf];
}

/*
 *	Invert byte order within each 16-bits of an array.
 */
static void
two_byte_swap(unsigned char *buf, int nbytes)
{
	unsigned char c;

	for (; nbytes > 0; nbytes -= 2, buf += 2) {
		c = buf[0];
		buf[0] = buf[1];
		buf[1] = c;
	}
}

#define FILEP FILE*
#define FREAD(a,b,c) fread(b,1,c,a)

/* read an 8 bit byte*/
static uint16
readINT8(FILEP file)
{
	unsigned char b;

	FREAD(file, &b, sizeof(b));
	return b;
}

/* read a 16-bit integer LSB16 format*/
static uint16
readLSB16(FILEP file)
{
	uint16 s;

	FREAD(file, &s, sizeof(s));
	return wswap(s);
}

/* read a 32-bit integer LSB32 format*/
static uint32
readLSB32(FILEP file)
{
	uint32 n;

	FREAD(file, &n, sizeof(n));
	return dwswap(n);
}

/* Get the offset of the given field */
static int
pcf_get_offset(int item)
{
	int i;

	for (i = 0; i < toc_size; i++)
		if (item == toc[i].type)
			return toc[i].offset;
	return -1;
}

/* Read the actual bitmaps into memory */
static int
pcf_readbitmaps(FILE *file, unsigned char **bits, int *bits_size,
	uint32 **offsets)
{
	long offset;
	uint32 format;
	uint32 num_glyphs;
	uint32 pad;
	unsigned int i;
	int endian;
	uint32 *o;
	unsigned char *b;
	uint32 bmsize[GLYPHPADOPTIONS];

	if ((offset = pcf_get_offset(PCF_BITMAPS)) == -1)
		return -1;
	fseek (file, offset, SEEK_SET);

	format = readLSB32(file);
	endian = (format & PCF_BIT_MASK)? PCF_LSB_FIRST: PCF_MSB_FIRST;

	num_glyphs = readLSB32(file);

	o = *offsets = (uint32 *)malloc(num_glyphs * sizeof(uint32));
	for (i=0; i < num_glyphs; ++i)
		o[i] = readLSB32(file);

	for (i=0; i < GLYPHPADOPTIONS; ++i)
		bmsize[i] = readLSB32(file);

	pad = format & PCF_GLYPH_PAD_MASK;
	*bits_size = bmsize[pad]? bmsize[pad] : 1;

	/* alloc and read bitmap data*/
	b = *bits = (unsigned char *) malloc(*bits_size);
	FREAD(file, b, *bits_size);

	/* convert bitmaps*/
	bit_order_invert(b, *bits_size);
#ifdef _BIG_ENDIAN
	if (endian == PCF_LSB_FIRST)
		two_byte_swap(b, *bits_size);
#else
	if (endian == PCF_MSB_FIRST)
		two_byte_swap(b, *bits_size);
#endif
	return num_glyphs;
}

/* read character metric data*/
static int
pcf_readmetrics(FILE * file, struct metric_entry **metrics)
{
	long i, size, offset;
	uint32 format;
	struct metric_entry *m;

	if ((offset = pcf_get_offset(PCF_METRICS)) == -1)
		return -1;
	fseek (file, offset, SEEK_SET);

	format = readLSB32(file);

	if ((format & PCF_FORMAT_MASK) == PCF_DEFAULT_FORMAT) {
		size = readLSB32(file);		/* 32 bits - Number of metrics*/

		m = *metrics = (struct metric_entry *) malloc(size *
			sizeof(struct metric_entry));

		for (i=0; i < size; i++) {
			m[i].leftBearing = readLSB16(file);
			m[i].rightBearing = readLSB16(file);
			m[i].width = readLSB16(file);
			m[i].ascent = readLSB16(file);
			m[i].descent = readLSB16(file);
			m[i].attributes = readLSB16(file);
		}
	} else {
		size = readLSB16(file);		/* 16 bits - Number of metrics*/

		m = *metrics = (struct metric_entry *) malloc(size *
			sizeof(struct metric_entry));

		for (i = 0; i < size; i++) {
			m[i].leftBearing = readINT8(file) - 0x80;
			m[i].rightBearing = readINT8(file) - 0x80;
			m[i].width = readINT8(file) - 0x80;
			m[i].ascent = readINT8(file) - 0x80;
			m[i].descent = readINT8(file) - 0x80;
		}
	}
	return size;
}

/* read encoding table*/
static int
pcf_read_encoding(FILE * file, struct encoding_entry **encoding)
{
	long offset, n;
	uint32 format;
	struct encoding_entry *e;

	if ((offset = pcf_get_offset(PCF_BDF_ENCODINGS)) == -1)
		return -1;
	fseek (file, offset, SEEK_SET);

	format = readLSB32(file);

	e = *encoding = (struct encoding_entry *)
		malloc(sizeof(struct encoding_entry));
	e->min_byte2 = readLSB16(file);
	e->max_byte2 = readLSB16(file);
	e->min_byte1 = readLSB16(file);
	e->max_byte1 = readLSB16(file);
	e->defaultchar = readLSB16(file);
	e->count = (e->max_byte2 - e->min_byte2 + 1) *
		(e->max_byte1 - e->min_byte1 + 1);
	e->map = (uint16 *) malloc(e->count * sizeof(uint16));

	for (n = 0; n < e->count; ++n) {
		e->map[n] = readLSB16(file);
		/*DPRINTF("ncode %x (%c) %x\n", n, n, e->map[n]);*/
	}
	return e->count;
}

static int
pcf_read_toc(FILE * file, struct toc_entry **toc, uint32 *size)
{
	long i;
	uint32 version;
	struct toc_entry *t;

	fseek (file, 0, SEEK_SET);

	/* Verify the version */
	version = readLSB32(file);
	if (version != PCF_FILE_VERSION)
		return -1;

	*size = readLSB32(file);
	t = *toc = (struct toc_entry *) calloc(sizeof(struct toc_entry), *size);
	if (!t)
		return -1;

	/* Read in the entire table of contents */
	for (i=0; i<*size; ++i) {
		t[i].type = readLSB32(file);
		t[i].format = readLSB32(file);
		t[i].size = readLSB32(file);
		t[i].offset = readLSB32(file);
	}

	return 0;
}

hd_font *HD_Font_LoadPCF (const char *fname) 
{
    FILE *file = 0;
    int offset, i, count, bsize, bwidth, err = 0;
    struct metric_entry *metrics = 0;
    struct encoding_entry *encoding = 0;
    uint16 *output;
    uint8 *glyphs = 0;
    uint32 *glyphs_offsets = 0;
    int max_width = 0, max_descent = 0, max_ascent = 0, max_height = 0;
    int glyph_count;
    uint32 *goffset = 0;
    unsigned char *gwidth = 0;
    
    file = fopen (fname, "rb");
    if (!file) {
        fprintf (stderr, "Could not open PCF font file %s: %s\n", fname, strerror (errno));
        return 0;
    }


    /* Read the table of contents */
    if (pcf_read_toc(file, &toc, &toc_size) == -1) {
	err = EINVAL;
	goto leave_func;
    }
    
    /* Now, read in the bitmaps */
    glyph_count = pcf_readbitmaps(file, &glyphs, &bsize, &glyphs_offsets);
    
    if (glyph_count == -1) {
	err = EINVAL;
	goto leave_func;
    }
    
    if (pcf_read_encoding(file, &encoding) == -1) {
	err = EINVAL;
	goto leave_func;
    }

    hd_font *font = malloc (sizeof(hd_font));
    font->firstchar = encoding->min_byte2 * (encoding->min_byte1 + 1);
    
    count = pcf_readmetrics (file, &metrics);
    
    for (i = 0; i < count; i++) {
	if (metrics[i].width > max_width)
	    max_width = metrics[i].width;
	if (metrics[i].ascent > max_ascent)
	    max_ascent = metrics[i].ascent;
	if (metrics[i].descent > max_descent)
	    max_descent = metrics[i].descent;
    }
    max_height = max_ascent + max_descent;
    
    font->bitstype    = HD_FONT_CLUMPED;
    font->pitch       = 2;
    font->h           = max_height;
    font->bpp         = 1;

    bwidth = (max_width + 15) / 16;

    font->pixels = calloc (1, (font->pixbytes = max_height * 2 * bwidth * glyph_count));
    if (!font->pixels) {
        err = ENOMEM;
        goto leave_func;
    }
    
    goffset = malloc (glyph_count * 4);
    gwidth = malloc (glyph_count);
    output = (uint16 *)font->pixels;
    offset = 0;

    /* copy and convert from packed BDF format to MWCFONT format*/
    for (i = 0; i < glyph_count; i++) {
	int h, w;
	int y = max_height;
	uint32 *ptr =
	    (uint32 *) (glyphs + glyphs_offsets[i]);
	
	/* # words image width*/
	int lwidth = (metrics[i].width + 15) / 16;
	
	/* # words image width, corrected for bounding box problem*/
	int xwidth = (metrics[i].rightBearing - metrics[i].leftBearing + 15) / 16;
	
	gwidth[i] = (unsigned char) metrics[i].width;
	goffset[i] = offset;
	
	offset += (lwidth * max_height);
	
	for (h = 0; h < (max_ascent - metrics[i].ascent); h++) {
	    for (w = 0; w < lwidth; w++)
		*output++ = 0;
	    y--;
	}
	
	for (h = 0; h < (metrics[i].ascent + metrics[i].descent); h++) {
	    uint16 *val = (uint16 *) ptr;
	    int            bearing, carry_shift;
	    uint16 carry = 0;
	    
	    /* leftBearing correction*/
	    bearing = metrics[i].leftBearing;
	    if (bearing < 0)	/* negative bearing not handled yet*/
		bearing = 0;
	    carry_shift = 16 - bearing;
	    
	    for (w = 0; w < lwidth; w++) {
		*output++ = (val[w] >> bearing) | carry;
		carry = val[w] << carry_shift;
	    }
	    ptr += (xwidth + 1) / 2;
	    y--;
	}
	
	for (; y > 0; y--)
	    for (w = 0; w < lwidth; w++)
		*output++ = 0;
    }    

    
    /* reorder offsets and width according to encoding map */
    font->offset = malloc(encoding->count * 4);
    font->width = malloc(encoding->count);
    for (i = 0; i < encoding->count; ++i) {
	uint16 n = encoding->map[i];
	if (n == 0xffff)	/* map non-existent chars to default char */
	    n = encoding->map[encoding->defaultchar];
	font->offset[i] = goffset[n] * 2;
	font->width[i] = gwidth[n];
    }
    
    font->nchars = encoding->count;

 leave_func:
    if (goffset)
	free(goffset);
    if (gwidth)
	free(gwidth);
    if (encoding) {
	if (encoding->map)
	    free(encoding->map);
	free(encoding);
    }
    if (metrics)
	free(metrics);
    if (glyphs)
	free(glyphs);
    if (glyphs_offsets)
	free(glyphs_offsets);
    
    if (toc)
	free(toc);
    toc = 0;
    toc_size = 0;
    
    if (file)
	fclose (file);
    
    if (err == 0 && font)
	return font;

    fprintf (stderr, "Error loading PCF font file %s: %s\n", fname, strerror (err));
    return 0;
}

hd_font *HD_Font_LoadSFont (const char *fname)
{
	hd_font *font;
	int iw, ih, x, i;
	hd_surface srf;
	uint32 pink, *p;
	uint8 *buf, *b;

	if (!(srf = HD_PNG_Load(fname, &iw, &ih)))
		return NULL;

	pink = HD_RGB(255, 0, 255);

	font = malloc(sizeof(hd_font));
	font->bitstype = HD_FONT_PITCHED;
	font->pitch = iw;
	font->h = ih - 1;
	font->pixbytes = (ih - 1) * iw;
	font->pixels = HD_SRF_ROWF(srf, 1);
	font->bpp = 8;
	font->firstchar = 32;
	font->offset = malloc(512 * sizeof(uint32));
	font->width = malloc(512 * sizeof(uint8));
	font->defaultchar = 32;
	font->offset[0] = 0;

	for (i = x = 0; x < iw; ++x) {
		if (HD_SRF_PIXF(srf, x, 0) == pink) {
			font->width[i] = x - font->offset[i];
			while (x < iw && HD_SRF_PIXF(srf, x, 0) == pink) ++x;
			if (x < iw) font->offset[++i] = x;
		}
	}
	font->width[i] = x - font->offset[i];
	font->nchars = i+1;
	font->offset = realloc(font->offset, (i+1) * sizeof(uint32));
	font->width = realloc(font->width, (i+1) * sizeof(uint8));

	buf = malloc((ih - 1) * iw);
	for (p = (uint32 *)font->pixels, b = buf; b - buf < (ih - 1) * iw; ++b)
		*b = (*p++ >> 24) & 0xff;
	font->pixels = buf;
	free(srf);

	return font;
}

static int h2d(char *s, int len)
{
	int i, ret = 0;
	for (i = 0; i < len; ++i) {
		ret <<= 4;
		if (*s >= 'A' && *s <= 'F') ret |= *s - 'A' + 0xa;
		if (*s >= '0' && *s <= '9') ret |= *s - '0';
		if (*s >= 'a' && *s <= 'f') ret |= *s - 'a' + 0xa;
		++s;
	}
	return ret;
}

hd_font *HD_Font_LoadFFF (const char *fname)
{
	char tmp[1024], res;
	hd_font *font;
	FILE *ip;
	long n_bits = 0;
	int i, w, lc = 0;

	if ((ip = fopen(fname, "r")) == NULL)
		return NULL;

	font = malloc(sizeof(hd_font));
	font->bitstype = HD_FONT_CLUMPED;
	font->pitch = 2;
	font->pixels = NULL;
	font->nchars = 0;
	font->firstchar = 0xffff;
	font->bpp = 1;

	fgets(tmp, 1024, ip);
	res = sscanf(tmp, "\\size %dx%d", &w, (int *)&font->h);
	res += sscanf(tmp, "%d%d", &w, (int *)&font->h);
	if (res != 2) {
		free(font);
		fclose(ip);
		return NULL;
	}
	while (fgets(tmp, 1024, ip)) {
		unsigned short index = h2d(tmp, 4);
		if (index < font->firstchar) font->firstchar = index;
		if (index > lc) lc = index;
		if (n_bits < index + 1) {
			n_bits = (index + 0x100) & ~0xff;
			font->pixels = realloc(font->pixels, n_bits*font->h*2);
		}
		for (i = 0; i < (int)font->h; ++i)
			*((unsigned short *)font->pixels + index*font->h + i) =
				h2d(tmp + 5 + (i * 2), 2) << 8;
	}
	font->defaultchar = font->firstchar;
	font->nchars = lc - font->firstchar;
	font->pixbytes = font->nchars * font->h * 2;
	font->offset = malloc(font->nchars * sizeof(uint32));
	font->width = malloc(font->nchars * sizeof(uint8));
	for (i = 0; i <= font->nchars; ++i) {
		font->offset[i] = (i + font->firstchar) * font->h * 2;
		font->width[i] = w;
	}
	fclose(ip);
	return font;
}
