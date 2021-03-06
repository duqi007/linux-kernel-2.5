/*
 *  linux/arch/arm/tools/getconsdata.c
 *
 *  Copyright (C) 1995-2001 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/config.h>
#include <linux/sched.h>
#include <linux/mm.h>

#include <asm/pgtable.h>
#include <asm/uaccess.h>

/*
 * Make sure that the compiler and target are compatible.
 */
#if defined(__APCS_32__) && defined(CONFIG_CPU_26)
#error Sorry, your compiler targets APCS-32 but this kernel requires APCS-26
#endif
#if defined(__APCS_26__) && defined(CONFIG_CPU_32)
#error Sorry, your compiler targets APCS-26 but this kernel requires APCS-32
#endif
#if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 95)
#error Sorry, your compiler is known to miscompile kernels.  Only use gcc 2.95.3 and later.
#endif
#if __GNUC__ == 2 && __GNUC_MINOR__ == 95
/* shame we can't detect the .1 or .2 releases */
#warning GCC 2.95.2 and earlier miscompiles kernels.
#endif

#define OFF_TSK(n) (unsigned long)&(((struct task_struct *)0)->n)
#define OFF_VMA(n) (unsigned long)&(((struct vm_area_struct *)0)->n)

#define DEFN(name,off) asm("\n#define "name" %0" :: "I" (off))

void func(void)
{
DEFN("TSK_USED_MATH",		OFF_TSK(used_math));
DEFN("TSK_ACTIVE_MM",		OFF_TSK(active_mm));

DEFN("VMA_VM_MM",		OFF_VMA(vm_mm));
DEFN("VMA_VM_FLAGS",		OFF_VMA(vm_flags));

DEFN("VM_EXEC",			VM_EXEC);

#ifdef CONFIG_CPU_32
DEFN("HPTE_TYPE_SMALL",		PTE_TYPE_SMALL);
DEFN("HPTE_AP_READ",		PTE_AP_READ);
DEFN("HPTE_AP_WRITE",		PTE_AP_WRITE);

DEFN("LPTE_PRESENT",		L_PTE_PRESENT);
DEFN("LPTE_YOUNG",		L_PTE_YOUNG);
DEFN("LPTE_BUFFERABLE",		L_PTE_BUFFERABLE);
DEFN("LPTE_CACHEABLE",		L_PTE_CACHEABLE);
DEFN("LPTE_USER",		L_PTE_USER);
DEFN("LPTE_WRITE",		L_PTE_WRITE);
DEFN("LPTE_EXEC",		L_PTE_EXEC);
DEFN("LPTE_DIRTY",		L_PTE_DIRTY);
#endif

#ifdef CONFIG_CPU_26
DEFN("PAGE_PRESENT",		_PAGE_PRESENT);
DEFN("PAGE_READONLY",		_PAGE_READONLY);
DEFN("PAGE_NOT_USER",		_PAGE_NOT_USER);
DEFN("PAGE_OLD",		_PAGE_OLD);
DEFN("PAGE_CLEAN",		_PAGE_CLEAN);
#endif

DEFN("PAGE_SZ",			PAGE_SIZE);

DEFN("SYS_ERROR0",		0x9f0000);
}
