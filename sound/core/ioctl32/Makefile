#
# Makefile for ALSA
# Copyright (c) 1999 by Jaroslav Kysela <perex@suse.cz>
#

O_TARGET     := _ioctl32.o

snd-ioctl32-objs := ioctl32.o pcm32.o rawmidi32.o timer32.o hwdep32.o
ifeq ($(CONFIG_SND_SEQUENCER),y)
  snd-ioctl32-objs += seq32.o
endif

obj-$(CONFIG_SND_BIT32_EMUL) += snd-ioctl32.o

include $(TOPDIR)/Rules.make
