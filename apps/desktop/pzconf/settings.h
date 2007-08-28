#ifndef _SETTINGS_H_
#define _SETTINGS_H_

/** Configuration stuff - config.c **/
#ifndef NODEF_CONFIG
typedef struct _pz_Config PzConfig;
#endif

extern PzConfig *pz_global_config;

PzConfig *pz_load_config (const char *filename);
void pz_save_config (PzConfig *conf);
void pz_blast_config (PzConfig *conf);
void pz_free_config (PzConfig *conf);

typedef struct _pz_ConfItem 
{
    unsigned int sid;

#define PZ_SETTING_INT     1
#define PZ_SETTING_STRING  2
#define PZ_SETTING_FLOAT   3
#define PZ_SETTING_ILIST   4
#define PZ_SETTING_SLIST   5
#define PZ_SETTING_BLOB    255
    int type;

    union 
    {
	int ival;
	char *strval;
	double fval;
	struct { int *ivals; int nivals; };
	struct { char **strvals; int nstrvals; };
	struct { void *blobval; int bloblen; };
    };
    struct _pz_ConfItem *next;
} PzConfItem;

#define PZ_SID_FORMAT  0xe0000000

PzConfItem *pz_get_setting (PzConfig *conf, unsigned int sid);
void pz_unset_setting (PzConfig *conf, unsigned int sid);
void pz_config_iterate (PzConfig *conf, void (*fn)(PzConfItem *));

int pz_get_int_setting (PzConfig *conf, unsigned int sid);
const char *pz_get_string_setting (PzConfig *conf, unsigned int sid);
void pz_set_int_setting (PzConfig *conf, unsigned int sid, int val);
void pz_set_string_setting (PzConfig *conf, unsigned int sid, const char *val);
void pz_set_float_setting (PzConfig *conf, unsigned int sid, double val);
void pz_set_ilist_setting (PzConfig *conf, unsigned int sid, int *vals, int nval);
void pz_set_slist_setting (PzConfig *conf, unsigned int sid, char **vals, int nval);
void pz_set_blob_setting (PzConfig *conf, unsigned int sid, void *val, int bytes);

#endif
