#
# Makefile for the Linux IEEE 1394 implementation
#

O_TARGET := ieee1394drv.o

export-objs := ieee1394_core.o ohci1394.o cmp.o

ieee1394-objs := ieee1394_core.o ieee1394_transactions.o hosts.o \
		 highlevel.o csr.o nodemgr.o

obj-$(CONFIG_IEEE1394) += ieee1394.o
obj-$(CONFIG_IEEE1394_PCILYNX) += pcilynx.o
obj-$(CONFIG_IEEE1394_OHCI1394) += ohci1394.o
obj-$(CONFIG_IEEE1394_VIDEO1394) += video1394.o
obj-$(CONFIG_IEEE1394_RAWIO) += raw1394.o
obj-$(CONFIG_IEEE1394_SBP2) += sbp2.o
obj-$(CONFIG_IEEE1394_DV1394) += dv1394.o
obj-$(CONFIG_IEEE1394_ETH1394) += eth1394.o
obj-$(CONFIG_IEEE1394_AMDTP) += amdtp.o
obj-$(CONFIG_IEEE1394_CMP) += cmp.o

include $(TOPDIR)/Rules.make
