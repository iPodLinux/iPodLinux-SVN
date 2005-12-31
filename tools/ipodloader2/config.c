#include "bootloader.h"
#include "minilibc.h"
#include "vfs.h"

#include "config.h"

static config_t config;

static const char *find_somewhere (const char **names, const char *what)
{
    int fd = -1;
    mlc_printf (">> Looking for a %s...\n", what);
    for (; *names; names++) {
        fd = vfs_open ((char *)*names);
        if (fd >= 0) break;
    }
    if (*names) {
        mlc_printf (">> Found it at %s.\n", *names);
    } else {
        int i;
        mlc_printf (">>! Not found. :-(\n\n");
        for (i = 0; i < 1000000; i++)
            ;
    }
    return *names;
}

void      config_init(void) {
    const char *confnames[] = { "(hd0,1)/LOADER.CFG", "(hd0,1)/loader.cfg",
                                "(hd0,1)/IPODLO~1.CON", "(hd0,1)/ipodloader.conf",
                                "(hd0,2)/ipodloader.conf", "(hd0,2)/loader.cfg", 0 };
    const char *kernnames[] = { "(hd0,1)/KERNEL.BIN", "(hd0,1)/kernel.bin",
                                "(hd0,1)/LINUX.BIN", "(hd0,1)/linux.bin",
                                "(hd0,1)/NOTES/KERNEL.BIN", "(hd0,1)/Notes/kernel.bin",
                                "(hd0,1)/vmlinux", "(hd0,2)/kernel.bin",
                                "(hd0,2)/linux.bin", "(hd0,2)/vmlinux", 0 };
    char *configdata, *p;
    int fd, len;
    const char *confname = find_somewhere (confnames, "configuration file");
    if (!confname || (fd = vfs_open ((char *)confname)) < 0) {
        config.image = (config_image_t *)mlc_malloc( sizeof(config_image_t) * 3 );
        
        config.timeout = 10;
        config.def     =  0;
        config.items   =  3;
        
        config.image[0].type  = CONFIG_IMAGE_BINARY;
        config.image[0].title = "RetailOS";
        config.image[0].path  = "(hd0,0)/osos";
        
        config.image[1].type  = CONFIG_IMAGE_BINARY;
        config.image[1].title = "iPodLinux";
        config.image[1].path  = (char *)find_somewhere (kernnames, "kernel");
        
        config.image[2].type  = CONFIG_IMAGE_SPECIAL;
        config.image[2].title = "Diskmode";
        config.image[2].path  = "diskmode";

        if (!config.image[1].path) {
            config.image[1].type = config.image[2].type;
            config.image[1].title = config.image[2].title;
            config.image[1].path = config.image[2].path;
            config.items = 2;
        }

        return;
    }

    configdata = mlc_malloc (4096);
    config.image = (config_image_t *)mlc_malloc( sizeof(config_image_t) * 8 );
    config.timeout = 10;
    config.def     = 0;
    config.items   = 0;
    if ((len = vfs_read (configdata, 4096, 1, fd)) == 4096) {
        mlc_printf ("Config file is too long, reading only first 4k\n");
    }
    configdata[len] = 0;
    p = configdata;
    while (*p) {
        char *nextline = mlc_strchr (p, '\n');
        char *value, *keyend;
        
        if (nextline) {
            *nextline = 0;
            do nextline++; while (*nextline == '\n');
        }

        while (*p == ' ' || *p == '\t') p++;

        if ((value = mlc_strchr (p, '=')) || (value = mlc_strchr (p, '@')) || (value = mlc_strchr (p, ' '))) {
            *value = 0;
            do value++; while (*value == ' ' || *value == '\t' || *value == '=' || *value == '@');
        }
        if (!value) {
            p = nextline;
            continue;
        }

        keyend = p;
        while (*keyend) keyend++; // goes to the NUL
        do keyend--; while (*keyend == ' ' || *keyend == '\t'); // goes to last nonblank
        *++keyend = 0; // first blank at end becomes end of string
        
        if (!mlc_strcmp (p, "default")) {
            config.def = *value - '0';
        } else if (!mlc_strcmp (p, "timeout")) {
            config.timeout = 0;
            while (*value >= '0' && *value <= '9') {
                config.timeout *= 10;
                config.timeout += *value - '0';
                value++;
            }
        } else {
            config.image[config.items].type  = CONFIG_IMAGE_BINARY;
            config.image[config.items].title = p;
            config.image[config.items].path  = value;
            if (!mlc_strcmp (value, "diskmode") || !mlc_strcmp (value, "diskscan") || !mlc_strcmp (value, "reboot"))
                config.image[config.items].type = CONFIG_IMAGE_SPECIAL;
            config.items++;
            if (config.items >= 8)
                break;
        }

        p = nextline;
    }
}

config_t *config_get(void) {
  return(&config);
}
