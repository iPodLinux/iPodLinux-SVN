/*
 *  arch/e1nommu/platform/E132XS/my_initrd.S
 *
 *  Copyright (C) 2002 GDT,  George Thanos<george.thanos@gdt.gr>
 *                           Yannis Mitsos<yannis.mitsos@gdt.gr>
 */
.section .initrd
.global __sinitrd
.global __einitrd

__sinitrd:
.include "initrdgz.img"
__einitrd:
