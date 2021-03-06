#ifndef _ASMARM_PAGE_H
#define _ASMARM_PAGE_H

#include <linux/config.h>

#ifdef __KERNEL__
#ifndef __ASSEMBLY__

#include <asm/glue.h>

struct cpu_user_fns {
	void (*cpu_clear_user_page)(void *p, unsigned long user);
	void (*cpu_copy_user_page)(void *to, const void *from,
				   unsigned long user);
};

#ifdef MULTI_USER
extern struct cpu_user_fns cpu_user;

#define __cpu_clear_user_page	cpu_user.cpu_clear_user_page
#define __cpu_copy_user_page	cpu_user.cpu_copy_user_page

#else

#define __cpu_clear_user_page	__glue(_USER,_clear_user_page)
#define __cpu_copy_user_page	__glue(_USER,_copy_user_page)

extern void __cpu_clear_user_page(void *p, unsigned long user);
extern void __cpu_copy_user_page(void *to, const void *from,
				 unsigned long user);
#endif

#define clear_user_page(addr,vaddr)			\
	do {						\
		preempt_disable();			\
		__cpu_clear_user_page(addr, vaddr);	\
		preempt_enable();			\
	} while (0)

#define copy_user_page(to,from,vaddr)			\
	do {						\
		preempt_disable();			\
		__cpu_copy_user_page(to, from, vaddr);	\
		preempt_enable();			\
	} while (0)

#define clear_page(page)	memzero((void *)(page), PAGE_SIZE)
extern void copy_page(void *to, void *from);

#undef STRICT_MM_TYPECHECKS

#ifdef STRICT_MM_TYPECHECKS
/*
 * These are used to make use of C type-checking..
 */
typedef struct { unsigned long pte; } pte_t;
typedef struct { unsigned long pmd; } pmd_t;
typedef struct { unsigned long pgprot; } pgprot_t;

#define pte_val(x)      ((x).pte)
#define pmd_val(x)      ((x).pmd)
#define pgprot_val(x)   ((x).pgprot)

#define __pte(x)        ((pte_t) { (x) } )
#define __pmd(x)        ((pmd_t) { (x) } )
#define __pgprot(x)     ((pgprot_t) { (x) } )

#else
/*
 * .. while these make it easier on the compiler
 */
typedef unsigned long pte_t;
typedef unsigned long pmd_t;
typedef unsigned long pgprot_t;

#define pte_val(x)      (x)
#define pmd_val(x)      (x)
#define pgprot_val(x)   (x)

#define __pte(x)        (x)
#define __pmd(x)        (x)
#define __pgprot(x)     (x)

#endif /* STRICT_MM_TYPECHECKS */
#endif /* !__ASSEMBLY__ */
#endif /* __KERNEL__ */

#include <asm/proc/page.h>

#define PAGE_SIZE		(1UL << PAGE_SHIFT)
#define PAGE_MASK		(~(PAGE_SIZE-1))

/* to align the pointer to the (next) page boundary */
#define PAGE_ALIGN(addr)	(((addr)+PAGE_SIZE-1)&PAGE_MASK)

#ifdef __KERNEL__
#ifndef __ASSEMBLY__

#ifdef CONFIG_DEBUG_BUGVERBOSE
extern void __bug(const char *file, int line, void *data);

/* give file/line information */
#define BUG()		__bug(__FILE__, __LINE__, NULL)
#define PAGE_BUG(page)	__bug(__FILE__, __LINE__, page)

#else

/* these just cause an oops */
#define BUG()		(*(int *)0 = 0)
#define PAGE_BUG(page)	(*(int *)0 = 0)

#endif

/* Pure 2^n version of get_order */
static inline int get_order(unsigned long size)
{
	int order;

	size = (size-1) >> (PAGE_SHIFT-1);
	order = -1;
	do {
		size >>= 1;
		order++;
	} while (size);
	return order;
}

#endif /* !__ASSEMBLY__ */

#include <asm/arch/memory.h>

#define __pa(x)			__virt_to_phys((unsigned long)(x))
#define __va(x)			((void *)__phys_to_virt((unsigned long)(x)))

#ifndef CONFIG_DISCONTIGMEM
#define virt_to_page(kaddr)	(mem_map + (__pa(kaddr) >> PAGE_SHIFT) - \
				 (PHYS_OFFSET >> PAGE_SHIFT))
#define VALID_PAGE(page)	((page - mem_map) < max_mapnr)
#endif

#define VM_DATA_DEFAULT_FLAGS	(VM_READ | VM_WRITE | VM_EXEC | \
				 VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

#endif /* __KERNEL__ */

#endif
