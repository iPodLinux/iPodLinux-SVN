#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "settings.h"

int mode = 0;
int num_printed = 0;
char value[1024];
int valopt = 0;
PzConfig *conf;

#define READ 1
#define WRITE 2
#define UNSET 3

void print_setting_sub (PzConfItem *ci) 
{
    int i;
    int first = 1;
    if (!ci) {
	printf ("<not set>");
	return;
    }
    switch (ci->type) {
    case PZ_SETTING_INT:
	printf ("  int %-6d", ci->ival);
	break;
    case PZ_SETTING_STRING:
	printf ("  str \"%s\"", ci->strval);
	break;
    case PZ_SETTING_FLOAT:
	printf ("float %-10.4f", ci->fval);
	break;
    case PZ_SETTING_ILIST:
	printf ("ilist[%d] (", ci->nivals);
	for (i = 0; i < ci->nivals; i++) {
	    printf ("%s%d", first? "" : ", ", ci->ivals[i]);
	    first = 0;
	}
	printf (")");
	break;
    case PZ_SETTING_SLIST:
	printf ("slist[%d] [", ci->nstrvals);
	for (i = 0; i < ci->nstrvals; i++) {
	    printf ("%s\"%s\"", first? "" : ", ", ci->strvals[i]);
	    first = 0;
	}
	printf ("]");
	break;
    case PZ_SETTING_BLOB:
	printf (" blob <%d>", ci->bloblen);
	break;
    }
}

void print_setting (PzConfItem *ci) 
{
    if (ci) {
	printf ("[%04d]: ", ci->sid);
	print_setting_sub (ci);
	num_printed++;
	printf ("\n");
    }
}

void handle_arg (unsigned int s, const char *v, int type) 
{
    PzConfItem *ci;

    switch (mode) {
    case READ:
	num_printed = 0;
	if (s == -1) {
	    pz_config_iterate (conf, print_setting);
	} else {
	    print_setting (pz_get_setting (conf, s));
	}
	if (!num_printed) {
	    if (s == -1)
		printf ("No settings.\n");
	    else
		printf ("[%04d]: <not set>\n", s);
	}
	break;
    case WRITE:
	printf ("[%04d]: ", s);
	if (ci = pz_get_setting (conf, s)) {
	    print_setting_sub (ci);
	    printf (" -> ");
	} else {
	    printf ("<not set>  -> ");
	}
	if (v) {
	    int *ivals, *ivalp;
	    char **svals, **svalp;
	    int nvals = 1;
	    const char *p; char *wv, *tok;
	    switch (type) {
	    case PZ_SETTING_INT:
		pz_set_int_setting (conf, s, atoi (v));
		break;
	    case PZ_SETTING_STRING:
		pz_set_string_setting (conf, s, v);
		break;
	    case PZ_SETTING_ILIST:
		nvals = 1;
		p = v;
		wv = strdup (v);
		while (*p) { if (*p == ',') nvals++; p++; }
		ivalp = ivals = calloc (nvals, sizeof(int));

		while ((tok = strsep (&wv, ","))) {
		    *ivalp++ = atoi (tok);
		}
		pz_set_ilist_setting (conf, s, ivals, nvals);
		break;
	    case PZ_SETTING_SLIST:
		nvals = 1;
		p = v;
		wv = strdup (v);
		while (*p) { if (*p == ',') nvals++; p++; }
		svalp = svals = calloc (nvals, sizeof(char*));
		while ((tok = strsep (&wv, ","))) {
		    *svalp++ = strdup (tok);
		}
		pz_set_slist_setting (conf, s, svals, nvals);
		break;
	    }
	    print_setting_sub (pz_get_setting (conf, s));
	    printf ("\n");
	} else {
	    printf ("<unspecified, no change>\n");
	}
	break;
    case UNSET:
	printf ("[%04d]: ", s);
	print_setting_sub (pz_get_setting (conf, s));
	printf (" -> ");
	pz_unset_setting (conf, s);
	print_setting_sub (pz_get_setting (conf, s));
	printf ("\n");
	break;
    }
}

void decode_arg (const char *arg, int guesstype) 
{
    const char *val = 0;
    int type;
    if (guesstype) {
	if (strchr (arg, '=')) {
	    if (isdigit (strchr (arg, '=')[1])) {
		type = PZ_SETTING_INT;
	    } else {
		type = PZ_SETTING_STRING;
	    }
	    if (strchr (arg, ','))
		type += 3; // list
	}
    }
    if (strchr (arg, '=')) {
	val = strchr (arg, '=') + 1;
    } else if (valopt) {
	val = value;
    } else {
	val = 0;
    }
    
    handle_arg (atoi (arg), val, type);
}

int main (int argc, char **argv) 
{
    int sid = -1;
    int type = 0;
    char filename[512];
    int gotfile = 0;
    int nonopts = 0;
    int c;

    while ((c = getopt (argc, argv, "i:f:t:v:ISlLrwu")) != -1) {
	switch (c) {
	case 'i': // setting id
	    sid = atoi (optarg);
	    break;
	case 'f': // file
	    strcpy (filename, optarg);
	    gotfile = 1;
	    break;
	case 't': // type
	    if (!strcmp (optarg, "int"))
		type = PZ_SETTING_INT;
	    else if (!strcmp (optarg, "string"))
		type = PZ_SETTING_STRING;
	    else if (!strcmp (optarg, "ilist"))
		type = PZ_SETTING_ILIST;
	    else if (!strcmp (optarg, "slist"))
		type = PZ_SETTING_SLIST;
	    else {
		fprintf (stderr, "Unknown type <%s> - must be `int' or `string' or `ilist', `slist'\n", optarg);
		return 1;
	    }
	    break;
	case 'I':
	    type = PZ_SETTING_INT;
	    break;
	case 'S':
	    type = PZ_SETTING_STRING;
	    break;
	case 'l':
	    type = PZ_SETTING_ILIST;
	    break;
	case 'L':
	    type = PZ_SETTING_SLIST;
	case 'v':
	    strcpy (value, optarg);
	    valopt = 1;
	    break;
	case 'r':
	    mode = READ;
	    break;
	case 'w':
	    mode = WRITE;
	    break;
	case 'u':
	    mode = UNSET;
	    break;
	}
    }

    argc -= optind;
    argv += optind;

    int idx = 0;
    if (!gotfile && (idx < argc)) {
	strcpy (filename, argv[idx++]);
	gotfile = 1;
    }
    if (strchr (filename, '=')) {
	gotfile = 0; // probably did pzconf -w ID=foo ID2=bar
    }
    if (!gotfile) {
	fprintf (stderr, "No file specified.\n"
		 "Usage: pzconf -f FILE [-i ID] [-t TYPE | -IS] [-v VALUE] [-rwu]\n"
		 "    or pzconf [-t TYPE | -IS] [-rwu] FILE [ID[=VALUE] ...]\n"
		 "    or some combination of the two\n\n");
	return 1;
    }
    if (mode == 0)
	mode = READ;

    conf = pz_load_config (filename);

    int guesstype = (type == 0) && (mode == WRITE);
    while (idx < argc) {
	decode_arg (argv[idx++], guesstype);
	nonopts++;
    }

    if (!nonopts) {
	if (mode == WRITE && !valopt) {
	    fprintf (stderr, "Write mode and no value specified.\n"
		     "Usage: pzconfedit -f FILE -i ID [-t TYPE | -IS] [-v VALUE] [-rwu]\n"
		     "    or pzconfedit [-t TYPE | -IS] [-rwu] FILE [ID[=VALUE] ...]\n"
		     "    or some combination of the two\n\n");
	}
	if (mode != READ && (sid == -1)) {
	    fprintf (stderr, "No ID specified.\n"
		     "Usage: pzconf -f FILE [-i ID] [-t TYPE | -IS] [-v VALUE] [-rwu]\n"
		     "    or pzconf [-t TYPE | -IS] [-rwu] FILE [ID[=VALUE] ...]\n"
		     "    or some combination of the two\n\n");
	    return 1;
	}
	if (mode != READ && !valopt) {
	    fprintf (stderr, "No value specified.\n"
		     "Usage: pzconf -f FILE [-i ID] [-t TYPE | -IS] [-v VALUE] [-rwu]\n"
		     "    or pzconf [-t TYPE | -IS] [-rwu] FILE [ID[=VALUE] ...]\n"
		     "    or some combination of the two\n\n");
	    return 1;
	}
	if (mode == WRITE && (type == 0)) {
	    if (isdigit (value[0]))
		type = PZ_SETTING_INT;
	    else
		type = PZ_SETTING_STRING;
	    if (strchr (value, ','))
		type += 3;
	}
	handle_arg (sid, value, type);
    }

    if ((mode == WRITE) || (mode == UNSET))
	pz_save_config (conf);

    return 0;
}
