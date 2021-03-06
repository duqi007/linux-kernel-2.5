#
# arch/arm/boot/Makefile
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1995-2002 Russell King
#

SYSTEM	=$(TOPDIR)/vmlinux

ifeq ($(CONFIG_CPU_26),y)
ZRELADDR	 = 0x02080000
PARAMS_PHYS	 = 0x0207c000
INITRD_PHYS	 = 0x02180000
INITRD_VIRT	 = 0x02180000
endif

ifeq ($(CONFIG_ARCH_RPC),y)
ZRELADDR	 = 0x10008000
PARAMS_PHYS	 = 0x10000100
INITRD_PHYS	 = 0x18000000
INITRD_VIRT	 = 0xc8000000
endif

ifeq ($(CONFIG_ARCH_CLPS7500),y)
ZRELADDR	 = 0x10008000
endif

ifeq ($(CONFIG_ARCH_EBSA110),y)
ZRELADDR	 = 0x00008000
PARAMS_PHYS	 = 0x00000400
INITRD_PHYS	 = 0x00800000
INITRD_VIRT	 = 0xc0800000
endif

ifeq ($(CONFIG_ARCH_SHARK),y)
ZTEXTADDR	 = 0x08508000
ZRELADDR	 = 0x08008000
endif

ifeq ($(CONFIG_FOOTBRIDGE),y)
ZRELADDR	 = 0x00008000
PARAMS_PHYS	 = 0x00000100
INITRD_PHYS	 = 0x00800000
INITRD_VIRT	 = 0xc0800000
endif

ifeq ($(CONFIG_ARCH_INTEGRATOR),y)
ZRELADDR	 = 0x00008000
PARAMS_PHYS	 = 0x00000100
INITRD_PHYS	 = 0x00800000
INITRD_VIRT	 = 0xc0800000
endif

ifeq ($(CONFIG_ARCH_CAMELOT),y)
ZRELADDR	 = 0x00008000
endif

ifeq ($(CONFIG_ARCH_NEXUSPCI),y)
ZRELADDR	 = 0x40008000
endif

ifeq ($(CONFIG_ARCH_L7200),y)
ZRELADDR	 = 0xf0008000
endif

# The standard locations for stuff on CLPS711x type processors
ifeq ($(CONFIG_ARCH_CLPS711X),y)
ZRELADDR	 = 0xc0028000
PARAMS_PHYS	 = 0xc0000100
endif

# Should probably have some agreement on these...
ifeq ($(CONFIG_ARCH_P720T),y)
INITRD_PHYS	 = 0xc0400000
INITRD_VIRT	 = 0xc0400000
endif
ifeq ($(CONFIG_ARCH_CDB89712),y)
INITRD_PHYS	 = 0x00700000
INITRD_VIRT	 = 0xc0300000
endif

ifeq ($(CONFIG_ARCH_SA1100),y)
ZRELADDR	 = 0xc0008000
# No defconfig file to move this into...
#ifeq ($(CONFIG_SA1100_YOPY),y)
#  ZTEXTADDR      = 0x00080000
#  ZBSSADDR       = 0xc0200000
#endif
ifeq ($(CONFIG_SA1111),y)
  ZRELADDR	 = 0xc0208000
endif
endif

ifeq ($(CONFIG_ARCH_ANAKIN),y)
ZRELADDR	 = 0x20008000
endif

ifeq ($(CONFIG_ARCH_IQ80310),y)
ZRELADDR	 = 0xa0008000
endif

ifeq ($(CONFIG_ARCH_ADIFCC),y)
ZRELADDR	 = 0xc0008000
endif

#
# We now have a PIC decompressor implementation.  Decompressors running
# from RAM should not define ZTEXTADDR.  Decompressors running directly
# from ROM or Flash must define ZTEXTADDR (preferably via the config)
#
ifeq ($(CONFIG_ZBOOT_ROM),y)
ZTEXTADDR	=0x$(CONFIG_ZBOOT_ROM_TEXT)
ZBSSADDR	=0x$(CONFIG_ZBOOT_ROM_BSS)
else
ZTEXTADDR	=0
ZBSSADDR	=ALIGN(4)
endif

export	SYSTEM ZTEXTADDR ZBSSADDR ZRELADDR INITRD_PHYS INITRD_VIRT PARAMS_PHYS

Image:	$(CONFIGURE) $(SYSTEM)
	$(OBJCOPY) -O binary -R .note -R .comment -S $(SYSTEM) $@

bzImage: zImage

zImage:	$(CONFIGURE) compressed/vmlinux
	$(OBJCOPY) -O binary -R .note -R .comment -S compressed/vmlinux $@

bootpImage: bootp/bootp
	$(OBJCOPY) -O binary -R .note -R .comment -S bootp/bootp $@

compressed/vmlinux: $(TOPDIR)/vmlinux dep
	@$(MAKE) -C compressed vmlinux

bootp/bootp: zImage initrd
	@$(MAKE) -C bootp bootp

initrd:
	@test "$(INITRD_VIRT)" != "" || (echo This architecture does not support INITRD; exit -1)
	@test "$(INITRD)" != "" || (echo You must specify INITRD; exit -1)

install: $(CONFIGURE) Image
	sh ./install.sh $(VERSION).$(PATCHLEVEL).$(SUBLEVEL)$(EXTRAVERSION) Image $(TOPDIR)/System.map "$(INSTALL_PATH)"

zinstall: $(CONFIGURE) zImage
	sh ./install.sh $(VERSION).$(PATCHLEVEL).$(SUBLEVEL)$(EXTRAVERSION) zImage $(TOPDIR)/System.map "$(INSTALL_PATH)"

clean:
	$(RM) Image zImage bootpImage
	@$(MAKE) -C compressed clean
	@$(MAKE) -C bootp clean

dep:
