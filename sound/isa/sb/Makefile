#
# Makefile for ALSA
# Copyright (c) 2001 by Jaroslav Kysela <perex@suse.cz>
#

O_TARGET     := _sb.o

export-objs  := emu8000.o emu8000_synth.o sb_common.o sb8_main.o sb16_main.o sb16_csp.o

snd-sb-common-objs := sb_common.o sb_mixer.o
snd-sb8-dsp-objs := sb8_main.o sb8_midi.o
snd-sb16-dsp-objs := sb16_main.o
snd-sb16-csp-objs := sb16_csp.o
snd-sb8-objs := sb8.o
snd-sb16-objs := sb16.o
snd-sbawe-objs := sbawe.o emu8000.o
snd-emu8000-synth-objs := emu8000_synth.o emu8000_callback.o emu8000_patch.o
snd-es968-objs := es968.o

# Toplevel Module Dependency
obj-$(CONFIG_SND_ALS100) += snd-sb16-dsp.o snd-sb-common.o
obj-$(CONFIG_SND_CMI8330) += snd-sb16-dsp.o snd-sb-common.o
obj-$(CONFIG_SND_DT0197H) += snd-sb16-dsp.o snd-sb-common.o
obj-$(CONFIG_SND_SB8) += snd-sb8.o snd-sb8-dsp.o snd-sb-common.o
obj-$(CONFIG_SND_SB16) += snd-sb16.o snd-sb16-dsp.o snd-sb-common.o
obj-$(CONFIG_SND_SBAWE) += snd-sbawe.o snd-sb16-dsp.o snd-sb-common.o
obj-$(CONFIG_SND_ES968) += snd-es968.o snd-sb8-dsp.o snd-sb-common.o
obj-$(CONFIG_SND_ALS4000) += snd-sb-common.o
ifeq ($(CONFIG_SND_SB16_CSP),y)
  obj-$(CONFIG_SND_SB16) += snd-sb16-csp.o
  obj-$(CONFIG_SND_SBAWE) += snd-sb16-csp.o
endif
ifeq ($(subst m,y,$(CONFIG_SND_SEQUENCER)),y)
  obj-$(CONFIG_SND_SBAWE) += snd-emu8000-synth.o
endif

obj-m := $(sort $(obj-m))

include $(TOPDIR)/Rules.make
