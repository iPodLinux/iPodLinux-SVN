#include "ttk.h"
#include "appearance.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

static TApItem *ap_head;

static int hex2nyb (char c) 
{
    if (isdigit (c)) return (c - '0');
    if (isalpha (c)) return (toupper (c) - 'A' + 10);
    return 0;
}
void ttk_ap_load (const char *filename) 
{
    FILE *f = fopen (filename, "r");
    char *buf;
    char *topid = 0;
    struct {
	char *key;
	char *value;
    } defines[10];
    int nextdef = 0;
    int i;
    TApItem *c;

    if (!f) {
	perror (filename);
	return;
    }
	
    c = ap_head;
    while (c) {
	TApItem *t;
	t = c->next;
	free (c);
	c = t;
    }
    ap_head = 0;

    buf = malloc (256);
    while (fgets (buf, 256, f)) {
	char *p = buf;

	if (*p == '#')
	    continue;
	
	if (buf[strlen (buf) - 1] == '\n')
	    buf[strlen (buf) - 1] = 0;

	if (*p == '\\') {
	    p++;
	    if (!strncmp (p, "def ", 4)) {
		p += 4;
		if (!strchr (p, ' ')) {
		    fprintf (stderr, "Syntax error. Format: \\def key value.\n");
		    return;
		}
		if (nextdef >= 10) {
		    fprintf (stderr, "Too many \\def initions\n");
		    return;
		}
		defines[nextdef].key = strdup (p);
		defines[nextdef].value = strchr (defines[nextdef].key, ' ');
		while (isspace (*defines[nextdef].value))
		    defines[nextdef].value++;
		*strchr (defines[nextdef].key, ' ') = 0;
		nextdef++;
		// Only free key when we're done.
	    } else if (strncmp (p, "name ", 5) != 0) {
		fprintf (stderr, "Appearance Warning: Unrecognized \\-command `%s'. Ignored.\n", p);
	    }
	} else {
	    while (isspace (*p) && *p) p++;
	    if (strchr (p, ':')) {
		if (topid)
		    free (topid);
		topid = p;
		p = strchr (p, ':'); *p = 0; p++;
		topid = strdup (topid);
		while (isspace (*p) && *p) p++;
	    }
	    if (*p) {
		char *subid = 0;
		char *sep, *end;
		char firstpart[64], *fp;
		TApItem *ap;

		if (!topid) {
		    fprintf (stderr, "Appearance Error: First definition line must have a top-level ID (e.g. header: ). Aborted.\n");
		    return;
		}

		while ((sep = strstr (p, "=>")) != 0) {
		    end = strchr (p, ',');
		    if (!end) end = p + strlen (p);
		    
		    ap = malloc (sizeof(TApItem));
		    if (!ap_head)
			ap_head = ap;
		    else {
			c = ap_head;
			while (c->next) c = c->next;
			c->next = ap;
		    }

		    fp = firstpart;

		    while (p != sep) {
			if (isspace (*p) || (*p == '_') || (*p == '-')) {
			    p++;
			    continue;
			}
			
			*fp++ = *p++;
			if (fp >= (firstpart + 63)) {
			    fp = firstpart + 61;
			}
		    }
		    *fp = 0;

		    ap->name = malloc (strlen (topid) + strlen (firstpart) + 2);
		    strcpy (ap->name, topid);
		    strcat (ap->name, ".");
		    strcat (ap->name, firstpart);
		    ap->type = 0;
		    ap->next = 0;

		    p += 2; // skip the =>
		    while (isspace (*p)) p++;
		    
		    while (p < end) {
			char *thing = p;
			if (strchr (p, ' ') && strchr (p, ' ') < end) {
			    p = strchr (p, ' ') + 1;
			    while (isspace (*p)) ++p;
			    *strchr (thing, ' ') = 0;
			} else {
			    if (!*end) {
				p = end;
			    } else {
				p = end + 1;
				*end = 0;
			    }
			}

			if (*thing == '@') { // file
			    char *path = malloc (strlen (filename) + strlen (thing));
			    strcpy (path, filename);
			    if (strrchr (path, '/'))
				strcpy (strchr (path, '/') + 1, thing + 1);
			    else
				strcpy (path, thing + 1);
			    
			    printf ("Loading image from %s\n", path);
			    ap->img = ttk_load_image (path);
			    if (!ap->img) {
				fprintf (stderr, "Appearance Warning: Could not load image %s for %s. Ignored.\n",
					 path, ap->name);
			    } else {
				ap->type |= TTK_AP_IMAGE;
			    }
			} else if (*thing == '-' || *thing == '+') { // spacing
			    int sign = (*thing == '-')? -1 : 1;
			    ap->spacing = atoi (thing + 1) * sign;
			    ap->type |= TTK_AP_SPACING;
			} else if (*thing == '#' || isalpha (*thing)) { // color
			    char *color = thing;
			    if (*thing != '#') { // resolve definition
				for (i = 0; i < nextdef; i++) {
				    if (!strcmp (defines[i].key, thing)) {
					color = defines[i].value;
					break;
				    }
				}
				if (i >= nextdef) { // no match
				    fprintf (stderr, "Appearance Warning: Undefined color: %s. Ignored.\n",
					     thing);
				    color = 0;
				}
			    }
			    if (color) {
				if (*color != '#') {
				    fprintf (stderr, "Appearance Warning: Color %s (\\def %s), for %s, doesn't start with #. Ignored.\n",
					     color, thing, ap->name);
				} else {
				    int r, g, b;
				    r = g = b = 0;

				    color++;

				    r |= hex2nyb (*color++); r <<= 4;
				    r |= hex2nyb (*color++);
				    g |= hex2nyb (*color++); g <<= 4;
				    g |= hex2nyb (*color++);
				    b |= hex2nyb (*color++); b <<= 4;
				    b |= hex2nyb (*color++);

				    ap->color = ttk_makecol (r, g, b);
				    ap->type |= TTK_AP_COLOR;
				}
			    }
			} else {
			    fprintf (stderr, "Appearance Warning: Unrecognized parameter |%s| for %s. Ignored.\n",
				     thing, ap->name);
			}
		    }
		}
	    }
	}
    }

    free (buf);
}

TApItem *ttk_ap_get (const char *prop) 
{
    TApItem *cur = ap_head;
    while (cur) {
	if (!strcmp (cur->name, prop)) {
	    return cur;
	}
	cur = cur->next;
    }
    return 0;
}

TApItem empty = { "NO_SUCH_ITEM", 0, 0, 0, 0, 0 };

TApItem *ttk_ap_getx (const char *prop) 
{
    TApItem *ret = ttk_ap_get (prop);
    
    if (!ret) {
	ret = &empty;
	ret->color = ttk_makecol (0, 0, 0);
    }

    return ret;
}

void ttk_ap_hline (ttk_surface srf, TApItem *ap, int x1, int x2, int y) 
{
    ttk_color col;

    if (!ap) return;

    if (!(ap->type & TTK_AP_COLOR)) {
	fprintf (stderr, "Appearance Warning: Property %s, for hline %d-%d y %d, has no color. Using black.\n",
		 ap->name, x1, x2, y);
	col = ttk_makecol (BLACK);
    } else {
	col = ap->color;
    }
    
    if (ap->type & TTK_AP_SPACING) {
	y += ap->spacing;
    }

    ttk_line (srf, x1, y, x2, y, col);
}

void ttk_ap_vline (ttk_surface srf, TApItem *ap, int x, int y1, int y2) 
{
    ttk_color col;

    if (!ap) return; // just don't draw it if we don't know how to draw it

    if (!(ap->type & TTK_AP_COLOR)) {
	fprintf (stderr, "Appearance Warning: Property %s, for vline %d-%d x %d, has no color. Using black.\n",
		 ap->name, y1, y2, x);
	col = ttk_makecol_ex (BLACK, srf);
    } else {
	col = ap->color;
    }
    
    if (ap->type & TTK_AP_SPACING) {
	x += ap->spacing;
    }

    ttk_line (srf, x, y1, x, y2, col);
}

void ttk_ap_dorect (ttk_surface srf, TApItem *ap, int x1, int y1, int x2, int y2, int filled) 
{
    ttk_color col;
    ttk_surface img;
    int iscol = 1;

    if (!ap) return; // not an error
    
    if (x1 == x2 || y1 == y2) {
	ttk_line (srf, x1, y1, x2, y2, ap->color);
	return;
    }
    int tmp;
    if (x1 > x2) tmp = x1, x1 = x2, x2 = tmp;
    if (y1 > y2) tmp = y1, y1 = y2, y2 = tmp;

    int dospc = 1;
    if (ap->type & TTK_AP_SPACING) {
	if ((x1 + ap->spacing) > (x2 - ap->spacing))
	    dospc = 0;
	if ((y1 + ap->spacing) > (y2 - ap->spacing))
	    dospc = 0;
    }

    if (ap->type & TTK_AP_IMAGE)
	img = ap->img;
    else
	img = 0;
    
    if (ap->type & TTK_AP_COLOR)
	col = ap->color;
    else if (srf)
	iscol = 0;
    else
	col = ttk_makecol_ex (BLACK, srf);

    if (ap->type & TTK_AP_SPACING && dospc) {
	x1 += ap->spacing;
	y1 += ap->spacing;
	x2 -= ap->spacing;
	y2 -= ap->spacing;
    }
    
    if (img)
	ttk_blit_image_ex (img, 0, 0, x2-x1, y2-y1,
			   srf, x1, y1);
    
    if (iscol) {
	if (filled)
	    ttk_fillrect (srf, x1, y1, x2, y2, col);
	else
	    ttk_rect (srf, x1, y1, x2, y2, col);
    }
}

void ttk_ap_rect (ttk_surface srf, TApItem *ap, int x1, int y1, int x2, int y2) 
{ ttk_ap_dorect (srf, ap, x1, y1, x2, y2, 0); }

void ttk_ap_fillrect (ttk_surface srf, TApItem *ap, int x1, int y1, int x2, int y2) 
{ ttk_ap_dorect (srf, ap, x1, y1, x2, y2, 1); }
