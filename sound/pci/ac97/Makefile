#
# Makefile for ALSA
# Copyright (c) 2001 by Jaroslav Kysela <perex@suse.cz>
#

O_TARGET     := _ac97.o

export-objs  := ac97_codec.o ak4531_codec.o

snd-ac97-codec-objs := ac97_codec.o
snd-ak4531-codec-objs := ak4531_codec.o

# Toplevel Module Dependency
obj-$(CONFIG_SND_CS4281) += snd-ac97-codec.o
obj-$(CONFIG_SND_ENS1370) += snd-ak4531-codec.o
obj-$(CONFIG_SND_ENS1371) += snd-ac97-codec.o
obj-$(CONFIG_SND_ES1968) += snd-ac97-codec.o
obj-$(CONFIG_SND_FM801) += snd-ac97-codec.o
obj-$(CONFIG_SND_ICE1712) += snd-ac97-codec.o
obj-$(CONFIG_SND_INTEL8X0) += snd-ac97-codec.o
obj-$(CONFIG_SND_MAESTRO3) += snd-ac97-codec.o
obj-$(CONFIG_SND_VIA686) += snd-ac97-codec.o
obj-$(CONFIG_SND_VIA8233) += snd-ac97-codec.o
obj-$(CONFIG_SND_ALI5451) += snd-ac97-codec.o
obj-$(CONFIG_SND_CS46XX) += snd-ac97-codec.o
obj-$(CONFIG_SND_EMU10K1) += snd-ac97-codec.o
obj-$(CONFIG_SND_NM256) += snd-ac97-codec.o
obj-$(CONFIG_SND_TRIDENT) += snd-ac97-codec.o
obj-$(CONFIG_SND_YMFPCI) += snd-ac97-codec.o

obj-m := $(sort $(obj-m))

include $(TOPDIR)/Rules.make
