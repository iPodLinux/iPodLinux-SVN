#
# Makefile for the linux armnommu-specific parts of the memory manager.
#

obj-y				:= extable.o fault.o \
				   fault-common.o init.o ioremap.o mm.o

obj-$(CONFIG_MODULES)		+= proc-syms.o

obj-$(CONFIG_ALIGNMENT_TRAP)	+= alignment.o

abrt-$(CONFIG_CPU_ABRT_EV4)	:= abort-ev4
abrt-$(CONFIG_CPU_ABRT_EV4T)	:= abort-ev4t
abrt-$(CONFIG_CPU_ABRT_LV4T)	:= abort-lv4t
abrt-$(CONFIG_CPU_ABRT_EV5T)	:= abort-ev5t
abrt-$(CONFIG_CPU_ABRT_EV5TJ)	:= abort-ev5tj
# test if the abort handler defined.
ifneq ($(abrt-y),)
obj-y				+= $(abrt-y).o
endif

ifneq ($(CONFIG_ARCH_S3C44B0),y) 
cache-$(CONFIG_CPU_CACHE_V3)	:= cache-v3
endif
cache-$(CONFIG_CPU_CACHE_V4)	:= cache-v4
cache-$(CONFIG_CPU_CACHE_V4WT)	:= cache-v4wt
cache-$(CONFIG_CPU_CACHE_V4WB)	:= cache-v4wb
# test if the cache handler defiend.
ifneq ($(cache-y),)
obj-y				+= $(cache-y).o
endif

obj-$(CONFIG_CPU_S3C4510B)	+= proc-s3c4510b.o
obj-$(CONFIG_CPU_ARM610)	+= proc-arm6_7.o
obj-$(CONFIG_CPU_ARM710)	+= proc-arm6_7.o
obj-$(CONFIG_CPU_ARM720T)	+= proc-arm720.o
obj-$(CONFIG_CPU_ARM740T)	+= proc-arm740.o
obj-$(CONFIG_CPU_ARM920T)	+= proc-arm920.o
obj-$(CONFIG_CPU_ARM922T)	+= proc-arm922.o
obj-$(CONFIG_CPU_ARM925T)	+= proc-arm925.o
obj-$(CONFIG_CPU_ARM926T)	+= proc-arm926.o
obj-$(CONFIG_CPU_ARM940T)	+= proc-arm940.o
obj-$(CONFIG_CPU_ARM946E)	+= proc-arm946.o
obj-$(CONFIG_CPU_ARM1020)	+= proc-arm1020.o
obj-$(CONFIG_CPU_ARM1020E)	+= proc-arm1020e.o
obj-$(CONFIG_CPU_ARM1022)	+= proc-arm1022.o
obj-$(CONFIG_CPU_ARM1026)	+= proc-arm1026.o

# Spell out some dependencies that aren't automatically figured out
ifneq ($(abrt-y),)
$(obj)/$(abrt-y).o:		arch/arm/mm/$(abrt-y).S FORCE
	$(call if_changed_dep,as_o_S) 
endif
ifneq ($(cache-y),)
$(obj)/$(cache-y).o:		arch/arm/mm/$(cache-y).S FORCE
	$(call if_changed_dep,as_o_S)
endif
$(obj)/alignment.o:		arch/arm/mm/alignment.c FORCE
	$(call if_changed_dep,cc_o_c)
$(obj)/discontig.o:		arch/arm/mm/discontig.c FORCE
	$(call if_changed_dep,cc_o_c)

