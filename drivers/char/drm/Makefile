#
# Makefile for the drm device driver.  This driver provides support for the
# Direct Rendering Infrastructure (DRI) in XFree86 4.1.0 and higher.

O_TARGET	:= drm.o

gamma-objs  := gamma_drv.o gamma_dma.o
tdfx-objs   := tdfx_drv.o
r128-objs   := r128_drv.o r128_cce.o r128_state.o
mga-objs    := mga_drv.o mga_dma.o mga_state.o mga_warp.o
i810-objs   := i810_drv.o i810_dma.o
radeon-objs := radeon_drv.o radeon_cp.o radeon_state.o
ffb-objs    := ffb_drv.o ffb_context.o

obj-$(CONFIG_DRM_GAMMA) += gamma.o
obj-$(CONFIG_DRM_TDFX)	+= tdfx.o
obj-$(CONFIG_DRM_R128)	+= r128.o
obj-$(CONFIG_DRM_RADEON)+= radeon.o
obj-$(CONFIG_DRM_MGA)	+= mga.o
obj-$(CONFIG_DRM_I810)	+= i810.o
obj-$(CONFIG_DRM_FFB)   += ffb.o

include $(TOPDIR)/Rules.make
