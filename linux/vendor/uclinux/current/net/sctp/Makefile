#
# Makefile for SCTP support code.
#

O_TARGET := sctp.o

obj-$(CONFIG_IP_SCTP) += sctp.o

obj-y := endpointola.o output.o sm_make_chunk.o associola.o hashdriver.o \
	outqueue.o sm_sideeffect.o transport.o bind_addr.o input.o primitive.o \
	sm_statefuns.o tsnmap.o command.o inqueue.o proc.o sm_statetable.o \
	ulpevent.o protocol.o socket.o ulpqueue.o debug.o sla1.o ssnmap.o

ifeq ($(CONFIG_SCTP_ADLER32), y)
obj-y += adler32.o
else
obj-y += crc32c.o
endif

obj-$(CONFIG_SCTP_DBG_OBJCNT) += objcnt.o
obj-$(CONFIG_SYSCTL) += sysctl.o

obj-$(subst m,y,$(CONFIG_IPV6))	+= ipv6.o

sctp-objs := $(obj-y)

include $(TOPDIR)/Rules.make
