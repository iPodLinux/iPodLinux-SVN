#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "bootloader.h"

#define CONFIG_IMAGE_BINARY  0x00
#define CONFIG_IMAGE_SPECIAL 0x01
#define CONFIG_IMAGE_ROCKBOX 0x02

typedef struct {
  uint32 type;
  char  *title;
  char  *path;
} config_image_t;

typedef struct {
  uint32 timeout;
  uint32 def;
  uint32 items;

  config_image_t *image;

} config_t;

void      config_init(void);
config_t *config_get(void);

#endif
