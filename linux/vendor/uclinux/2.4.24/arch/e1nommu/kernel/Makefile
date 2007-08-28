#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

all: kernel.o

O_TARGET := kernel.o

obj-y := process.o semaphore.o setup.o init_task.o time.o traps.o signal.o ptrace.o irq.o sys_e1.o float_entry.o floating.o
#obj-y += signal.o
# ksyms.o - not needed because we don't support modules right now.
#ifdef CONFIG_CONSOLE
#obj-y += console.o
#endif

include $(TOPDIR)/Rules.make
