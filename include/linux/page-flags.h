/*
 * Macros for manipulating and testing page->flags
 */

#ifndef PAGE_FLAGS_H
#define PAGE_FLAGS_H

/*
 * Various page->flags bits:
 *
 * PG_reserved is set for special pages, which can never be swapped out. Some
 * of them might not even exist (eg empty_bad_page)...
 *
 * The PG_private bitflag is set if page->private contains a valid value.
 *
 * During disk I/O, PG_locked_dontuse is used. This bit is set before I/O and
 * reset when I/O completes. page_waitqueue(page) is a wait queue of all tasks
 * waiting for the I/O on this page to complete.
 *
 * PG_uptodate tells whether the page's contents is valid.  When a read
 * completes, the page becomes uptodate, unless a disk I/O error happened.
 *
 * For choosing which pages to swap out, inode pages carry a PG_referenced bit,
 * which is set any time the system accesses that page through the (mapping,
 * index) hash table.  This referenced bit, together with the referenced bit
 * in the page tables, is used to manipulate page->age and move the page across
 * the active, inactive_dirty and inactive_clean lists.
 *
 * Note that the referenced bit, the page->lru list_head and the active,
 * inactive_dirty and inactive_clean lists are protected by the
 * pagemap_lru_lock, and *NOT* by the usual PG_locked_dontuse bit!
 *
 * PG_error is set to indicate that an I/O error occurred on this page.
 *
 * PG_arch_1 is an architecture specific page state bit.  The generic code
 * guarantees that this bit is cleared for a page when it first is entered into
 * the page cache.
 *
 * PG_highmem pages are not permanently mapped into the kernel virtual address
 * space, they need to be kmapped separately for doing IO on the pages.  The
 * struct page (these bits with information) are always mapped into kernel
 * address space...
 */

/*
 * Don't use the *_dontuse flags.  Use the macros.  Otherwise you'll break
 * locked- and dirty-page accounting.  The top eight bits of page->flags are
 * used for page->zone, so putting flag bits there doesn't work.
 */
#define PG_locked_dontuse	 0	/* Page is locked. Don't touch. */
#define PG_error		 1
#define PG_referenced		 2
#define PG_uptodate		 3

#define PG_dirty_dontuse	 4
#define PG_lru			 5
#define PG_active		 6
#define PG_slab			 7	/* slab debug (Suparna wants this) */

#define PG_highmem		 8
#define PG_checked		 9	/* kill me in 2.5.<early>. */
#define PG_arch_1		10
#define PG_reserved		11

#define PG_launder		12	/* written out by VM pressure.. */
#define PG_private		13	/* Has something at ->private */
#define PG_writeback		14	/* Page is under writeback */

/*
 * Global page accounting.  One instance per CPU.
 */
extern struct page_state {
	unsigned long nr_dirty;
	unsigned long nr_locked;
	unsigned long nr_pagecache;
} ____cacheline_aligned_in_smp page_states[NR_CPUS];

extern void get_page_state(struct page_state *ret);

#define mod_page_state(member, delta)					\
	do {								\
		preempt_disable();					\
		page_states[smp_processor_id()].member += (delta);	\
		preempt_enable();					\
	} while (0)

#define inc_page_state(member)	mod_page_state(member, 1UL)
#define dec_page_state(member)	mod_page_state(member, 0UL - 1)


/*
 * Manipulation of page state flags
 */
#define PageLocked(page)	test_bit(PG_locked_dontuse, &(page)->flags)
#define SetPageLocked(page)						\
	do {								\
		if (!test_and_set_bit(PG_locked_dontuse,		\
				&(page)->flags))			\
			inc_page_state(nr_locked);			\
	} while (0)
#define TestSetPageLocked(page)						\
	({								\
		int ret;						\
		ret = test_and_set_bit(PG_locked_dontuse,		\
					&(page)->flags);		\
		if (!ret)						\
			inc_page_state(nr_locked);			\
		ret;							\
	})
#define ClearPageLocked(page)						\
	do {								\
		if (test_and_clear_bit(PG_locked_dontuse,		\
				&(page)->flags))			\
			dec_page_state(nr_locked);			\
	} while (0)
#define TestClearPageLocked(page)					\
	({								\
		int ret;						\
		ret = test_and_clear_bit(PG_locked_dontuse,		\
				&(page)->flags);			\
		if (ret)						\
			dec_page_state(nr_locked);			\
		ret;							\
	})

#define PageError(page)		test_bit(PG_error, &(page)->flags)
#define SetPageError(page)	set_bit(PG_error, &(page)->flags)
#define ClearPageError(page)	clear_bit(PG_error, &(page)->flags)

#define PageReferenced(page)	test_bit(PG_referenced, &(page)->flags)
#define SetPageReferenced(page)	set_bit(PG_referenced, &(page)->flags)
#define ClearPageReferenced(page)	clear_bit(PG_referenced, &(page)->flags)
#define TestClearPageReferenced(page) test_and_clear_bit(PG_referenced, &(page)->flags)

#define PageUptodate(page)	test_bit(PG_uptodate, &(page)->flags)
#define SetPageUptodate(page)	set_bit(PG_uptodate, &(page)->flags)
#define ClearPageUptodate(page)	clear_bit(PG_uptodate, &(page)->flags)

#define PageDirty(page)		test_bit(PG_dirty_dontuse, &(page)->flags)
#define SetPageDirty(page)						\
	do {								\
		if (!test_and_set_bit(PG_dirty_dontuse,			\
					&(page)->flags))		\
			inc_page_state(nr_dirty);			\
	} while (0)
#define TestSetPageDirty(page)						\
	({								\
		int ret;						\
		ret = test_and_set_bit(PG_dirty_dontuse,		\
				&(page)->flags);			\
		if (!ret)						\
			inc_page_state(nr_dirty);			\
		ret;							\
	})
#define ClearPageDirty(page)						\
	do {								\
		if (test_and_clear_bit(PG_dirty_dontuse,		\
				&(page)->flags))			\
			dec_page_state(nr_dirty);			\
	} while (0)
#define TestClearPageDirty(page)					\
	({								\
		int ret;						\
		ret = test_and_clear_bit(PG_dirty_dontuse,		\
				&(page)->flags);			\
		if (ret)						\
			dec_page_state(nr_dirty);			\
		ret;							\
	})

#define PageLRU(page)		test_bit(PG_lru, &(page)->flags)
#define TestSetPageLRU(page)	test_and_set_bit(PG_lru, &(page)->flags)
#define TestClearPageLRU(page)	test_and_clear_bit(PG_lru, &(page)->flags)

#define PageActive(page)	test_bit(PG_active, &(page)->flags)
#define SetPageActive(page)	set_bit(PG_active, &(page)->flags)
#define ClearPageActive(page)	clear_bit(PG_active, &(page)->flags)

#define PageSlab(page)		test_bit(PG_slab, &(page)->flags)
#define SetPageSlab(page)	set_bit(PG_slab, &(page)->flags)
#define ClearPageSlab(page)	clear_bit(PG_slab, &(page)->flags)

#ifdef CONFIG_HIGHMEM
#define PageHighMem(page)	test_bit(PG_highmem, &(page)->flags)
#else
#define PageHighMem(page)	0 /* needed to optimize away at compile time */
#endif

#define PageChecked(page)	test_bit(PG_checked, &(page)->flags)
#define SetPageChecked(page)	set_bit(PG_checked, &(page)->flags)

#define PageReserved(page)	test_bit(PG_reserved, &(page)->flags)
#define SetPageReserved(page)	set_bit(PG_reserved, &(page)->flags)
#define ClearPageReserved(page)	clear_bit(PG_reserved, &(page)->flags)

#define PageLaunder(page)	test_bit(PG_launder, &(page)->flags)
#define SetPageLaunder(page)	set_bit(PG_launder, &(page)->flags)

#define SetPagePrivate(page)	set_bit(PG_private, &(page)->flags)
#define ClearPagePrivate(page)	clear_bit(PG_private, &(page)->flags)
#define PagePrivate(page)	test_bit(PG_private, &(page)->flags)

#define PageWriteback(page)	test_bit(PG_writeback, &(page)->flags)
#define SetPageWriteback(page)	set_bit(PG_writeback, &(page)->flags)
#define ClearPageWriteback(page) clear_bit(PG_writeback, &(page)->flags)
#define TestSetPageWriteback(page)	\
	test_and_set_bit(PG_writeback, &(page)->flags)
#define TestClearPageWriteback(page)	\
	test_and_clear_bit(PG_writeback, &(page)->flags)

/*
 * The PageSwapCache predicate doesn't use a PG_flag at this time,
 * but it may again do so one day.
 */
extern struct address_space swapper_space;
#define PageSwapCache(page) ((page)->mapping == &swapper_space)

#endif	/* PAGE_FLAGS_H */
