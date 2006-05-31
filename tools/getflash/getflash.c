/*
 * getflash.c
 *
 * Written May 31, 06 by Thomas Tempelmann
 *
 * Purpose:
 *   To be run on an iPod under iPodLinux
 *   Saves the Flash ROM contents to the file "flashrom.bin"
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main (int argc, char **argv)
{
  (void)argc; // gets rid of "unused arg" warning

  FILE *fp;
  if ((fp = fopen ("flashrom.bin", "wb")) == NULL) {
    perror(argv[0]);
    exit(2);
  }

  // map the Flash ROM to 0x20000000
  //
  // (note: this assumes that the 4th mapping register is unused,
  //  which is the case with current kernel versions.)
  *(volatile long*)0xf000f018 = 0x3a00 | 0x20000000; // logical addr
  *(volatile long*)0xf000f01c = 0x3f84 | 0; // physical addr

  // copy 1MB from the ROM to the file, the current size of the Flash ROM (in 1G to 5G iPods):
  fwrite ((void*)0x20000000, 0x1000, 0x100, fp);

  // remove the mapping again
  *(volatile long*)0xf000f018 = 0; // logical addr
  *(volatile long*)0xf000f01c = 0; // physical addr

  fclose (fp);

  return 0;
}
