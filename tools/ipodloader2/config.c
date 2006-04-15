#include "bootloader.h"
#include "minilibc.h"
#include "console.h"
#include "vfs.h"

#include "config.h"

#define DEBUGPRINT(x) { mlc_printf (x); mlc_delay_ms (2000); }

static config_t config;

static const char * find_somewhere (const char **names, const char *what, int *fdOut)
{
    int fd = -1;
#if DEBUG
    mlc_printf (">> Looking for a %s...\n", what);
#endif
    for (; *names; names++) {
#if DEBUG
        mlc_printf (">> Trying |%s|...\n", *names);
#endif
        fd = vfs_open ((char *)*names);
        if (fd >= 0) break;
    }
#if DEBUG
    if (*names) {
        mlc_printf (">> Found it at %s.\n", *names);
    } else {
        mlc_printf (">>! Not found. :-(\n");
    }
#endif
    if (fdOut) *fdOut = fd;
    return *names;
}

const char *confnames[] = { "(hd0,1)/LOADER.CFG", "(hd0,1)/IPODLO~1.CON",
                            "(hd0,2)/boot/loader.cfg", "(hd0,2)/boot/ipodloader.conf",
                            "(hd0,2)/loader.cfg", "(hd0,2)/ipodloader.conf", 0 };
const char *kernnames[] = { "(hd0,1)/KERNEL.BIN", "(hd0,1)/LINUX.BIN",
                            "(hd0,1)/NOTES/KERNEL.BIN", "(hd0,1)/VMLINUX",
                            "(hd0,2)/boot/kernel.bin", "(hd0,2)/boot/linux.bin",
                            "(hd0,2)/boot/vmlinux", "(hd0,2)/kernel.bin",
                            "(hd0,2)/linux.bin", "(hd0,2)/vmlinux", 0 };

#define MAXITEMS 10 // more does not work as long as the assignment to "config.def" below is not fixed

static config_image_t configimgs[MAXITEMS];

void config_init(void) {
    char *configdata, *p;
    int fd, len, i;

    mlc_memset (&config, 0, sizeof (config));
    mlc_memset (&configimgs, 0, sizeof (configimgs));
    config.image = configimgs;
    config.timeout = 10;
    config.def     = 0;
    
    find_somewhere (confnames, "configuration file", &fd);

    if (fd < 0) {
        // no config file was found -> use defaults
    
      setDefaultSettings:
        i = 0;
 
        config.image[i].type  = CONFIG_IMAGE_BINARY;
        config.image[i].title = "Apple OS";
        if (vfs_open ("(hd0,0)/aple") >= 0) {
          config.image[i].path = "(hd0,0)/aple";
        } else {
          config.image[i].path = "(hd0,0)/osos";
        }
        i++;

        config.image[i].type  = CONFIG_IMAGE_BINARY;
        config.image[i].title = "iPodLinux";
        config.image[i].path  = (char *)find_somewhere (kernnames, "kernel", NULL);
        if (config.image[i].path) { i++; }

        config.image[i].type  = CONFIG_IMAGE_ROCKBOX;
        config.image[i].title = "Rockbox";
        config.image[i].path  = "(hd0,1)/rockbox.ipod";
        if (vfs_open (config.image[i].path) >= 0) {
          i++;
        } else {
          config.image[i].path  = "(hd0,1)/ROCKBO~1.IPO";
          if (vfs_open (config.image[i].path) >= 0) i++;
        }

        config.image[i].type  = CONFIG_IMAGE_SPECIAL;
        config.image[i].title = "Disk Mode";
        config.image[i].path  = "diskmode";
        i++;

        config.image[i].type  = CONFIG_IMAGE_SPECIAL;
        config.image[i].title = "Sleep";
        config.image[i].path  = "standby";
        i++;

        config.items = i;

        return;
    }

    // read the config file into the buffer at 'configdata'
    configdata = mlc_malloc (4096);
    mlc_memset (configdata, 0, 4096);
    if ((len = vfs_read (configdata, 1, 4096, fd)) == 4096) {
        mlc_printf ("Config file is too long, reading only first 4k\n");
        --len;
    }
    configdata[len] = 0;
    
    // change all CRs into LFs (for Windows and Mac users)
    p = configdata;
    while (*p) {
      if (*p == '\r') *p = '\n';
      ++p;
    }
    
    // now parse the file contents
    p = configdata;
    while (p && *p) {
        char *nextline = mlc_strchr (p, '\n');
        char *value, *keyend;

        if (nextline) {
            *nextline = 0;
            do { nextline++; } while (*nextline == '\n'); // skips empty lines
        }

        while (*p == ' ' || *p == '\t') p++;

        if ((value = mlc_strchr (p, '@')) != 0 || (value = mlc_strchr (p, '=')) != 0 || (value = mlc_strchr (p, ' ')) != 0) {
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
            config.def = mlc_atoi (value);
        } else if (!mlc_strcmp (p, "timeout")) {
            config.timeout = 0;
            config.timeout = mlc_atoi (value);
            if (config.timeout < 3) config.timeout = 3; // less than 3s is unusable currently with the slow (polling) keypress code
        } else if (!mlc_strcmp (p, "debug")) {
            config.debug = mlc_atoi (value);
        } else {
            config.image[config.items].type  = CONFIG_IMAGE_BINARY;
            config.image[config.items].title = p;
            config.image[config.items].path  = value;
            if (!mlc_strncasecmp (value, "rb:", 3)) {
                config.image[config.items].path += 3;
                config.image[config.items].type = CONFIG_IMAGE_ROCKBOX;
            } else if (value[0] != '(') { // no partition specifier -> it's not a path but command
                config.image[config.items].type = CONFIG_IMAGE_SPECIAL;
            }
            config.items++;
            if (config.items >= MAXITEMS) {
                break;
            }
        }

        p = nextline;
    }
    
    // finally, some sanity checks:
    if (config.items == 0) goto setDefaultSettings;
    if (config.def > config.items) config.def = config.items;
}

config_t *config_get(void) {
  return(&config);
}
