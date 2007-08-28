/*
 *  arch/e1nommu/platform/E132XS/initstack.S
 *
 *  Copyright (C) 2002 GDT,  George Thanos<george.thanos@gdt.gr>
 *                           Yannis Mitsos<yannis.mitsos@gdt.gr>
 */
.global __Init_Register_Stack_

.section .tasks

.align 13 
__Init_Register_Stack_:
.space   0x2000

