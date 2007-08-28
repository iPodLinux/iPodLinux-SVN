# Copyright (C) 2002 GDT, Yiannis Mitsos<yiannis.mitsos@gdt.gr>
# Put definitions for platforms and boards in here.

ifdef CONFIG_E132XS
PLATFORM := E132XS

ifdef CONFIG_E132XS_BOARD
BOARD := E132XS_BOARD
endif

endif


export BOARD
export PLATFORM

