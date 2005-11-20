#!/bin/sh

arm-elf-gcc -Wall -ffreestanding -nostdinc -c startup.s
arm-elf-gcc -Wall -ffreestanding -nostdinc -c loader.c
arm-elf-gcc -Wall -ffreestanding -nostdinc -c ata.c
arm-elf-gcc -Wall -ffreestanding -nostdinc -c fb.c
arm-elf-gcc -Wall -ffreestanding -nostdinc -c console.c
#arm-elf-gcc -Wall -ffreestanding -nostdinc -c minilibc.c
arm-elf-gcc -Wl,-Tarm_elf_40.x -nostartfiles -o loader.elf startup.o loader.o ata.o fb.o console.o
arm-elf-objcopy -O binary loader.elf loader.bin
./make_fw -g 4g -o my_sw.bin -i apple_os.bin loader.bin 