%{ /*-*-C-*- (well, lex really, but C works well enough) */

#include <ttk.h>
#include <string.h>
#define NUM_DEFS	(16)
static TApItem *ap_head = 0;

static int hex2nyb (char c) 
{
    if (isdigit (c)) return (c - '0');
    if (isalpha (c)) return (toupper (c) - 'A' + 10);
    return 0;
}
 
static struct {
    char *key;
    char *value;
} defines[NUM_DEFS];
static int nextdef;

static const char *filename;

extern int yylineno;
#define WARN(str...) do { fprintf(stderr, "%s:%d: warning: ", filename, yylineno); \
                         fprintf(stderr, str); fprintf(stderr, "\n"); } while (0)
#define ERR(str...)  do { fprintf(stderr, "%s:%d: error: ", filename, yylineno); \
                         fprintf(stderr, str); fprintf(stderr, "\n"); return 1; } while(0)

#define MAX_EXPANSION_DEPTH 10
static YY_BUFFER_STATE expansion_stack[MAX_EXPANSION_DEPTH];
static int expansion_stack_ptr = 0;

%}

%x COMMAND
%x DEFK DEFV
%x PREFIX
%x KEY
%x VALUE
%x GRADSTART GRADEND
%x SKIP

%option yylineno

ID	[a-zA-Z0-9._]+
HEX	[a-fA-F0-9]{6}
SHEX	[a-fA-F0-9]{3}
DEC	[0-9]+

%%
    int nextstate = INITIAL;
    char *fileprefix = 0;
    char *Topid = 0, *topid = 0, *subid = 0;
    TApItem *ap;
    
    expansion_stack_ptr = yylineno = 0;

<SKIP>[ \t]+
<SKIP>\n	BEGIN nextstate;
<SKIP>[^ \t]	yymore(); BEGIN nextstate;


<INITIAL>^"#".*\n	/* skip comments */



<INITIAL>^"\\"	BEGIN COMMAND;

<COMMAND>def	nextstate = DEFK; BEGIN SKIP;
<DEFK>{ID}	defines[nextdef].key = strdup (yytext); nextstate = DEFV; BEGIN SKIP;
<DEFK>.		WARN ("unrecognized character %s in definition key", yytext);
<DEFV>.+	defines[nextdef++].value = strdup (yytext);
<DEFV>\n	BEGIN INITIAL;

<COMMAND>name.*\n BEGIN INITIAL;

<COMMAND>prefix[ \t]+ BEGIN PREFIX;
<PREFIX>[^ \t]+	fileprefix = strdup (yytext); nextstate = INITIAL; BEGIN SKIP;



<INITIAL>{ID}: {
    if (yytext[0] == '.') {
        if (topid) free (topid);
        topid = malloc (strlen (Topid) + strlen (yytext) + 2);
        sprintf (topid, "%s.%s", Topid, yytext);
        topid[strlen (topid) - 1] = 0;
    } else {
        if (topid) free (topid);
        topid = strdup (yytext);
        topid[yyleng-1] = 0;
    }

    if (yytext[0] != '.') {
        if (Topid) free (Topid);
        Topid = strdup (topid);
    }

    BEGIN KEY;
}

<INITIAL,KEY>[ \t]+
<INITIAL,KEY>{ID} {
    if (subid) {
        WARN ("overriding id %s with %s", subid, yytext);
        free (subid);
    }
    subid = strdup (yytext);
    BEGIN KEY;
}
<KEY>\n		BEGIN INITIAL;
<KEY>"=>" {
    if (!topid) ERR ("first definition line missing top-level ID");
    if (!subid) ERR ("=> without key");
    
    ap = malloc (sizeof(TApItem));
    if (!ap_head)
        ap_head = ap;
    else {
        TApItem *c = ap_head;
        while (c->next) c = c->next;
        c->next = ap;
    }

    ap->name = malloc (strlen (topid) + strlen (subid) + 2);
    sprintf (ap->name, "%s.%s", topid, subid);
    free (subid);
    subid = 0;
    ap->type = 0;
    ap->next = 0;

    BEGIN VALUE;
}
.		WARN ("unrecognized character %s in key", yytext);

<VALUE,GRADSTART,GRADEND>[ \t]+

<VALUE,GRADSTART,GRADEND>"#"{HEX} {
    int r, g, b; r = g = b = 0;

    r |= hex2nyb (yytext[1]); r <<= 4;
    r |= hex2nyb (yytext[2]);
    g |= hex2nyb (yytext[3]); g <<= 4;
    g |= hex2nyb (yytext[4]);
    b |= hex2nyb (yytext[5]); b <<= 4;
    b |= hex2nyb (yytext[6]);
    
    switch (YY_START) {
        case VALUE:     if (ap->type & TTK_AP_COLOR) WARN ("redefining color for %s", ap->name);
                        ap->color = ttk_makecol (r, g, b); ap->type |= TTK_AP_COLOR; break;
        case GRADSTART: ap->gradstart = ttk_makecol (r, g, b); ap->type |= TTK_AP_GRADIENT; break;
        case GRADEND:   ap->gradend = ttk_makecol (r, g, b); break;
    }
}

<VALUE,GRADSTART,GRADEND>"#"{SHEX} {
    int r, g, b;
    
    r = hex2nyb (yytext[1]); r |= (r << 4);
    g = hex2nyb (yytext[2]); g |= (g << 4);
    b = hex2nyb (yytext[3]); b |= (b << 4);

    switch (YY_START) {
        case VALUE:     if (ap->type & TTK_AP_COLOR) WARN ("redefining color for %s", ap->name);
                        ap->color = ttk_makecol (r, g, b); ap->type |= TTK_AP_COLOR; break;
        case GRADSTART: ap->gradstart = ttk_makecol (r, g, b); ap->type |= TTK_AP_GRADIENT; break;
        case GRADEND:   ap->gradend = ttk_makecol (r, g, b); break;
    }
}

<GRADSTART,GRADEND>horiz(ontal)? ap->type |= TTK_AP_GRAD_HORIZ;
<GRADSTART,GRADEND>vert(ical)?	 ap->type &= ~TTK_AP_GRAD_HORIZ;

<VALUE>"<"	BEGIN GRADSTART;
<GRADSTART>(to|,) BEGIN GRADEND;
<GRADEND>(to|,)	{
    if (ap->type & TTK_AP_GRAD_MID)
        WARN ("too many colors for gradient for %s, only using first one and last two", ap->name);
    ap->type |= TTK_AP_GRAD_MID;
    ap->gradmid = ap->gradend;
}
<GRADEND>">"	BEGIN VALUE;

<VALUE>("-"|"+")[0-9]+ {
    if (ap->type & TTK_AP_SPACING)
        WARN ("overriding spacing for %s (%d) with %s", ap->name, ap->spacing, yytext);
    ap->type |= TTK_AP_SPACING;
    ap->spacing = atoi (yytext);
}

<VALUE>@[^ \t\n]+ {
    char *path;

    if (ap->type & TTK_AP_IMAGE)
        WARN ("overriding image for %s with %s", ap->name, yytext);
    ap->type |= TTK_AP_IMAGE;

    if (!fileprefix) fileprefix = strdup ("");

    path = malloc (strlen (filename) + strlen (fileprefix) + yyleng);
    strcpy (path, filename);
    if (strchr (filename, '/'))
        sprintf (strrchr (path, '/') + 1, "%s%s", fileprefix, yytext + 1);
    else
        sprintf (path, "%s%s", fileprefix, yytext + 1);

    ap->img = ttk_load_image (path);

    if (!ap->img) {
        WARN ("could not load image %s for %s - ignored\n", path, ap->name);
        ap->type &= ~TTK_AP_IMAGE;
    }

    free (path);
}

<VALUE>{DEC}"x"{DEC}"+"{DEC}"+"{DEC} {
    char *str = yytext;
    ap->type |= TTK_AP_RRECT;
    ap->rw = atoi (str); str = strchr (str, 'x') + 1;
    ap->rh = atoi (str); str = strchr (str, '+') + 1;
    ap->rx = atoi (str); str = strchr (str, '+') + 1;
    ap->ry = atoi (str);
}

<VALUE,GRADSTART,GRADEND>{ID} {
    char *str = 0;
    int i;
    for (i = 0; i < nextdef; i++) {
        if (!strcmp (defines[i].key, yytext)) {
            str = defines[i].value;
            break;
        }
    }
    if (!str) {
        WARN ("identifier %s has no \\def inition - did you mistype it?", yytext);
    } else {
        if (expansion_stack_ptr >= MAX_EXPANSION_DEPTH) {
            ERR ("Too many levels of \\def");
        }

        expansion_stack[expansion_stack_ptr++] = YY_CURRENT_BUFFER;
        yy_scan_string (str);
    }
}

<<EOF>> {
    if (--expansion_stack_ptr < 0) {
        yyterminate();
    } else {
        yy_delete_buffer (YY_CURRENT_BUFFER);
        yy_switch_to_buffer (expansion_stack[expansion_stack_ptr]);
    }
}

<VALUE>,	BEGIN KEY;
<VALUE>\n	BEGIN INITIAL;

<VALUE>{ID}	WARN ("bare identifier %s in value - did you mistype a \\def inition?", yytext);
<VALUE>.	WARN ("unrecognized character %s in value", yytext);
<*>.		WARN ("unrecognized character %s", yytext);
<INITIAL>\n	/* ignore blank lines */

%%

int yywrap() { return 1; }

void ttk_ap_load (const char *file)
{
    FILE *f = fopen (file, "r");
    TApItem *c;

    filename = file;

    if (!f) {
	perror (file);
	return;
    }
    
    nextdef = 0;

    c = ap_head;
    while (c) {
	TApItem *t;
	t = c->next;
        if (c->type & TTK_AP_IMAGE)
            ttk_free_surface (c->img);
        free (c->name);
	free (c);
	c = t;
    }
    ap_head = 0;

    yyrestart (f);
    yylex();

    for(;nextdef>0;nextdef--)
        if(defines[nextdef-1].key!=NULL)
            free(defines[nextdef-1].key);
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
    int tmp;

    if (!ap) return; // not an error
    
    if (x1 == x2 || y1 == y2) {
	ttk_line (srf, x1, y1, x2, y2, ap->color);
	return;
    }
    if (x1 > x2) tmp = x1, x1 = x2, x2 = tmp;
    if (y1 > y2) tmp = y1, y1 = y2, y2 = tmp;

    if (ap->type & TTK_AP_IMAGE)
	img = ap->img;
    else
	img = 0;
    
    if (ap->type & TTK_AP_COLOR)
	col = ap->color;
    else
	iscol = 0;

    if (ap->type & TTK_AP_SPACING) {
        int ox1 = x1, ox2 = x2, oy1 = y1, oy2 = y2;
	x1 += ap->spacing;
	y1 += ap->spacing;
	x2 -= ap->spacing;
	y2 -= ap->spacing;
        if (x1 >= x2 || y1 >= y2) {
            if ((ox2 - ox1) > (oy2 - oy1)) {
                x1 = ox1 + ap->spacing;
                x2 = ox2 - ap->spacing;
                y1 = oy1 + ap->spacing;
                y2 = y1 + 1;
            } else {
                x1 = ox1 + ap->spacing;
                x2 = x1 + 1;
                y1 = oy1 + ap->spacing;
                y2 = oy2 - ap->spacing;
            }
        }
    }
    
    if (ap->type & TTK_AP_GRADIENT) {
        void (*gradfn)(ttk_surface, int, int, int, int, ttk_color, ttk_color) = ttk_vgradient;
        if (ap->type & TTK_AP_GRAD_HORIZ)
            gradfn = ttk_hgradient;
        
        if (ap->type & TTK_AP_GRAD_MID) {
            if (gradfn == ttk_hgradient) {
                gradfn (srf, x1, y1, (x1 + x2) >> 1, y2, ap->gradstart, ap->gradmid);
                gradfn (srf, (x1 + x2) >> 1, y1, x2, y2, ap->gradmid, ap->gradend);
            } else {
                gradfn (srf, x1, y1, x2, (y1 + y2) >> 1, ap->gradstart, ap->gradmid);
                gradfn (srf, x1, (y1 + y2) >> 1, x2, y2, ap->gradmid, ap->gradend);
            }
        } else {
            gradfn (srf, x1, y1, x2, y2, ap->gradstart, ap->gradend);
        }
    }

    if (img) {
        // ...
	ttk_blit_image_ex (img, 0, 0, x2-x1, y2-y1,
			   srf, x1, y1);
    }
    
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
