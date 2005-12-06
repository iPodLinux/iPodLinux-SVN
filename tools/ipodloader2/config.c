#include "bootloader.h"
#include "minilibc.h"

#include "config.h"

static config_t config;

void      config_init(void) {
  config.image = (config_image_t *)mlc_malloc( sizeof(config_image_t) * 3 );

  config.timeout = 10;
  config.def     =  0;
  config.items   =  3;

  config.image[0].type  = CONFIG_IMAGE_BINARY;
  config.image[0].title = "RetailOS";
  config.image[0].path  = "(hd0,0)/osos";

  config.image[1].type  = CONFIG_IMAGE_BINARY;
  config.image[1].title = "uClinux";
  //config.image[1].path  = "(hd0,2)/kernel.ext";
  config.image[1].path  = "(hd0,1)/NOTES/KERNEL.BIN";

  config.image[2].type  = CONFIG_IMAGE_SPECIAL;
  config.image[2].title = "Diskmode";
  config.image[2].path  = "diskmode";

  return;
}

config_t *config_get(void) {
  return(&config);
}
