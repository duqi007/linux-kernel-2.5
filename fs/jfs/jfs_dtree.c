/*
 *   Copyright (c) International Business Machines Corp., 2000-2002
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or 
 *   (at your option) any later version.
 * 
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software 
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/*
 *	jfs_dtree.c: directory B+-tree manager
 *
 * B+-tree with variable length key directory:
 *
 * each directory page is structured as an array of 32-byte
 * directory entry slots initialized as a freelist
 * to avoid search/compaction of free space at insertion.
 * when an entry is inserted, a number of slots are allocated
 * from the freelist as required to store variable length data
 * of the entry; when the entry is deleted, slots of the entry
 * are returned to freelist.
 *
 * leaf entry stores full name as key and file serial number
 * (aka inode number) as data.
 * internal/router entry stores sufffix compressed name
 * as key and simple extent descriptor as data.
 *
 * each directory page maintains a sorted entry index table
 * which stores the start slot index of sorted entries
 * to allow binary search on the table.
 *
 * directory starts as a root/leaf page in on-disk inode
 * inline data area.
 * when it becomes full, it starts a leaf of a external extent
 * of length of 1 block. each time the first leaf becomes full,
 * it is extended rather than split (its size is doubled),
 * until its length becoms 4 KBytes, from then the extent is split
 * with new 4 Kbyte extent when it becomes full
 * to reduce external fragmentation of small directories.
 *
 * blah, blah, blah, for linear scan of directory in pieces by
 * readdir().
 *
 *
 *	case-insensitive directory file system
 *
 * names are stored in case-sensitive way in leaf entry.
 * but stored, searched and compared in case-insensitive (uppercase) order
 * (i.e., both search key and entry key are folded for search/compare):
 * (note that case-sensitive order is BROKEN in storage, e.g.,
 *  sensitive: Ad, aB, aC, aD -> insensitive: aB, aC, aD, Ad
 *
 *  entries which folds to the same key makes up a equivalent class
 *  whose members are stored as contiguous cluster (may cross page boundary)
 *  but whose order is arbitrary and acts as duplicate, e.g.,
 *  abc, Abc, aBc, abC)
 *
 * once match is found at leaf, requires scan forward/backward
 * either for, in case-insensitive search, duplicate
 * or for, in case-sensitive search, for exact match
 *
 * router entry must be created/stored in case-insensitive way
 * in internal entry:
 * (right most key of left page and left most key of right page
 * are folded, and its suffix compression is propagated as router
 * key in parent)
 * (e.g., if split occurs <abc> and <aBd>, <ABD> trather than <aB>
 * should be made the router key for the split)
 *
 * case-insensitive search:
 *
 * 	fold search key;
 *
 *	case-insensitive search of B-tree:
 *	for internal entry, router key is already folded;
 *	for leaf entry, fold the entry key before comparison.
 *
 *	if (leaf entry case-insensitive match found)
 *		if (next entry satisfies case-insensitive match)
 *			return EDUPLICATE;
 *		if (prev entry satisfies case-insensitive match)
 *			return EDUPLICATE;
 *		return match;
 *	else
 *		return no match;
 *
 * 	serialization:
 * target directory inode lock is being held on entry/exit
 * of all main directory service routines.
 *
 *	log based recovery:
 */

#include <linux/fs.h>
#include <linux/locks.h>
#include <linux/smp_lock.h>
#include "jfs_incore.h"
#include "jfs_superblock.h"
#include "jfs_filsys.h"
#include "jfs_metapage.h"
#include "jfs_dmap.h"
#include "jfs_unicode.h"
#include "jfs_debug.h"

/* dtree split parameter */
typedef struct {
	metapage_t *mp;
	s16 index;
	s16 nslot;
	component_t *key;
	ddata_t *data;
	pxdlist_t *pxdlist;
} dtsplit_t;

#define DT_PAGE(IP, MP) BT_PAGE(IP, MP, dtpage_t, i_dtroot)

/* get page buffer for specified block address */
#define DT_GETPAGE(IP, BN, MP, SIZE, P, RC)\
{\
	BT_GETPAGE(IP, BN, MP, dtpage_t, SIZE, P, RC, i_dtroot)\
	if (!(RC))\
	{\
		if (((P)->header.nextindex > (((BN)==0)?DTROOTMAXSLOT:(P)->header.maxslot)) ||\
		    ((BN) && ((P)->header.maxslot > DTPAGEMAXSLOT)))\
		{\
			jERROR(1,("DT_GETPAGE: dtree page corrupt\n"));\
			BT_PUTPAGE(MP);\
			updateSuper((IP)->i_sb, FM_DIRTY);\
			MP = NULL;\
			RC = EIO;\
		}\
	}\
}

/* for consistency */
#define DT_PUTPAGE(MP) BT_PUTPAGE(MP)

#define DT_GETSEARCH(IP, LEAF, BN, MP, P, INDEX) \
	BT_GETSEARCH(IP, LEAF, BN, MP, dtpage_t, P, INDEX, i_dtroot)

/*
 * forward references
 */
static int dtSplitUp(tid_t tid, struct inode *ip,
		     dtsplit_t * split, btstack_t * btstack);

static int dtSplitPage(tid_t tid, struct inode *ip, dtsplit_t * split,
		       metapage_t ** rmpp, dtpage_t ** rpp, pxd_t * rxdp);

static int dtExtendPage(tid_t tid, struct inode *ip,
			dtsplit_t * split, btstack_t * btstack);

static int dtSplitRoot(tid_t tid, struct inode *ip,
		       dtsplit_t * split, metapage_t ** rmpp);

static int dtDeleteUp(tid_t tid, struct inode *ip, metapage_t * fmp,
		      dtpage_t * fp, btstack_t * btstack);

static int dtSearchNode(struct inode *ip,
			s64 lmxaddr, pxd_t * kpxd, btstack_t * btstack);

static int dtRelink(tid_t tid, struct inode *ip, dtpage_t * p);

static int dtReadFirst(struct inode *ip, btstack_t * btstack);

static int dtReadNext(struct inode *ip,
		      loff_t * offset, btstack_t * btstack);

static int dtCompare(component_t * key, dtpage_t * p, int si);

static int ciCompare(component_t * key, dtpage_t * p, int si, int flag);

static void dtGetKey(dtpage_t * p, int i, component_t * key, int flag);

static void ciGetLeafPrefixKey(dtpage_t * lp, int li, dtpage_t * rp,
			       int ri, component_t * key, int flag);

static void dtInsertEntry(dtpage_t * p, int index, component_t * key,
			  ddata_t * data, dtlock_t ** dtlock);

static void dtMoveEntry(dtpage_t * sp, int si, dtpage_t * dp,
			dtlock_t ** sdtlock, dtlock_t ** ddtlock,
			int do_index);

static void dtDeleteEntry(dtpage_t * p, int fi, dtlock_t ** dtlock);

static void dtTruncateEntry(dtpage_t * p, int ti, dtlock_t ** dtlock);

static void dtLinelockFreelist(dtpage_t * p, int m, dtlock_t ** dtlock);

#define ciToUpper(c)	UniStrupr((c)->name)

/*
 *	find_index()
 *
 *	Returns dtree page containing directory table entry for specified
 *	index and pointer to its entry.
 *
 *	mp must be released by caller.
 */
static dir_table_slot_t *find_index(struct inode *ip, u32 index,
				    metapage_t ** mp)
{
	struct jfs_inode_info *jfs_ip = JFS_IP(ip);
	s64 blkno;
	s64 offset;
	int page_offset;
	dir_table_slot_t *slot;
	static int maxWarnings = 10;

	if (index < 2) {
		if (maxWarnings) {
			jERROR(1, ("find_entry called with index = %d\n",
				   index));
			maxWarnings--;
		}
		return 0;
	}

	if (index >= jfs_ip->next_index) {
		jFYI(1, ("find_entry called with index >= next_index\n"));
		return 0;
	}

	if (jfs_ip->next_index <= (MAX_INLINE_DIRTABLE_ENTRY + 1)) {
		/*
		 * Inline directory table
		 */
		*mp = 0;
		slot = &jfs_ip->i_dirtable[index - 2];
	} else {
		offset = (index - 2) * sizeof(dir_table_slot_t);
		page_offset = offset & (PSIZE - 1);
		blkno = ((offset + 1) >> L2PSIZE) <<
		    JFS_SBI(ip->i_sb)->l2nbperpage;

		if (*mp && ((*mp)->index != blkno)) {
			release_metapage(*mp);
			*mp = 0;
		}
		if (*mp == 0)
			*mp = read_metapage(ip, blkno, PSIZE, 0);
		if (*mp == 0) {
			jERROR(1,
			       ("free_index: error reading directory table\n"));
			return 0;
		}

		slot =
		    (dir_table_slot_t *) ((char *) (*mp)->data +
					  page_offset);
	}
	return slot;
}

static inline void lock_index(tid_t tid, struct inode *ip, metapage_t * mp,
			      u32 index)
{
	tlock_t *tlck;
	linelock_t *llck;
	lv_t *lv;

	tlck = txLock(tid, ip, mp, tlckDATA);
	llck = (linelock_t *) tlck->lock;

	if (llck->index >= llck->maxcnt)
		llck = txLinelock(llck);
	lv = &llck->lv[llck->index];

	/*
	 *      Linelock slot size is twice the size of directory table
	 *      slot size.  512 entries per page.
	 */
	lv->offset = ((index - 2) & 511) >> 1;
	lv->length = 1;
	llck->index++;
}

/*
 *	add_index()
 *
 *	Adds an entry to the directory index table.  This is used to provide
 *	each directory entry with a persistent index in which to resume
 *	directory traversals
 */
static u32 add_index(tid_t tid, struct inode *ip, s64 bn, int slot)
{
	struct super_block *sb = ip->i_sb;
	struct jfs_sb_info *sbi = JFS_SBI(sb);
	struct jfs_inode_info *jfs_ip = JFS_IP(ip);
	u64 blkno;
	dir_table_slot_t *dirtab_slot;
	u32 index;
	linelock_t *llck;
	lv_t *lv;
	metapage_t *mp;
	s64 offset;
	uint page_offset;
	int rc;
	tlock_t *tlck;
	s64 xaddr;

	ASSERT(DO_INDEX(ip));

	if (jfs_ip->next_index < 2) {
		jERROR(1, ("next_index = %d.  Please fix this!\n",
			   jfs_ip->next_index));
		jfs_ip->next_index = 2;
	}

	index = jfs_ip->next_index++;

	if (index <= MAX_INLINE_DIRTABLE_ENTRY) {
		/*
		 * i_size reflects size of index table, or 8 bytes per entry.
		 */
		ip->i_size = (loff_t) (index - 1) << 3;

		/*
		 * dir table fits inline within inode
		 */
		dirtab_slot = &jfs_ip->i_dirtable[index-2];
		dirtab_slot->flag = DIR_INDEX_VALID;
		dirtab_slot->slot = slot;
		DTSaddress(dirtab_slot, bn);

		set_cflag(COMMIT_Dirtable, ip);

		return index;
	}
	if (index == (MAX_INLINE_DIRTABLE_ENTRY + 1)) {
		/*
		 * It's time to move the inline table to an external
		 * page and begin to build the xtree
		 */

		/*
		 * Save the table, we're going to overwrite it with the
		 * xtree root
		 */
		dir_table_slot_t temp_table[12];
		memcpy(temp_table, &jfs_ip->i_dirtable, sizeof(temp_table));

		/*
		 * Initialize empty x-tree
		 */
		xtInitRoot(tid, ip);

		/*
		 * Allocate the first block & add it to the xtree
		 */
		xaddr = 0;
		if ((rc =
		     xtInsert(tid, ip, 0, 0, sbi->nbperpage,
			      &xaddr, 0))) {
			jFYI(1, ("add_index: xtInsert failed!\n"));
			return -1;
		}
		ip->i_size = PSIZE;
		ip->i_blocks += LBLK2PBLK(sb, sbi->nbperpage);

		if ((mp = get_metapage(ip, 0, ip->i_blksize, 0)) == 0) {
			jERROR(1, ("add_index: get_metapage failed!\n"));
			xtTruncate(tid, ip, 0, COMMIT_PWMAP);
			return -1;
		}
		tlck = txLock(tid, ip, mp, tlckDATA);
		llck = (linelock_t *) & tlck->lock;
		ASSERT(llck->index == 0);
		lv = &llck->lv[0];

		lv->offset = 0;
		lv->length = 6;	/* tlckDATA slot size is 16 bytes */
		llck->index++;

		memcpy(mp->data, temp_table, sizeof(temp_table));

		mark_metapage_dirty(mp);
		release_metapage(mp);

		/*
		 * Logging is now directed by xtree tlocks
		 */
		clear_cflag(COMMIT_Dirtable, ip);
	}

	offset = (index - 2) * sizeof(dir_table_slot_t);
	page_offset = offset & (PSIZE - 1);
	blkno = ((offset + 1) >> L2PSIZE) << sbi->l2nbperpage;
	if (page_offset == 0) {
		/*
		 * This will be the beginning of a new page
		 */
		xaddr = 0;
		if ((rc =
		     xtInsert(tid, ip, 0, blkno, sbi->nbperpage,
			      &xaddr, 0))) {
			jFYI(1, ("add_index: xtInsert failed!\n"));
			jfs_ip->next_index--;
			return -1;
		}
		ip->i_size += PSIZE;
		ip->i_blocks += LBLK2PBLK(sb, sbi->nbperpage);

		if ((mp = get_metapage(ip, blkno, PSIZE, 0)))
			memset(mp->data, 0, PSIZE);	/* Just looks better */
		else
			xtTruncate(tid, ip, offset, COMMIT_PWMAP);
	} else
		mp = read_metapage(ip, blkno, PSIZE, 0);

	if (mp == 0) {
		jERROR(1, ("add_index: get/read_metapage failed!\n"));
		return -1;
	}

	lock_index(tid, ip, mp, index);

	dirtab_slot =
	    (dir_table_slot_t *) ((char *) mp->data + page_offset);
	dirtab_slot->flag = DIR_INDEX_VALID;
	dirtab_slot->slot = slot;
	DTSaddress(dirtab_slot, bn);

	mark_metapage_dirty(mp);
	release_metapage(mp);

	return index;
}

/*
 *	free_index()
 *
 *	Marks an entry to the directory index table as free.
 */
static void free_index(tid_t tid, struct inode *ip, u32 index, u32 next)
{
	dir_table_slot_t *dirtab_slot;
	metapage_t *mp = 0;

	dirtab_slot = find_index(ip, index, &mp);

	if (dirtab_slot == 0)
		return;

	dirtab_slot->flag = DIR_INDEX_FREE;
	dirtab_slot->slot = dirtab_slot->addr1 = 0;
	dirtab_slot->addr2 = cpu_to_le32(next);

	if (mp) {
		lock_index(tid, ip, mp, index);
		mark_metapage_dirty(mp);
		release_metapage(mp);
	} else
		set_cflag(COMMIT_Dirtable, ip);
}

/*
 *	modify_index()
 *
 *	Changes an entry in the directory index table
 */
static void modify_index(tid_t tid, struct inode *ip, u32 index, s64 bn,
			 int slot, metapage_t ** mp)
{
	dir_table_slot_t *dirtab_slot;

	dirtab_slot = find_index(ip, index, mp);

	if (dirtab_slot == 0)
		return;

	DTSaddress(dirtab_slot, bn);
	dirtab_slot->slot = slot;

	if (*mp) {
		lock_index(tid, ip, *mp, index);
		mark_metapage_dirty(*mp);
	} else
		set_cflag(COMMIT_Dirtable, ip);
}

/*
 *	get_index()
 *
 *	reads a directory table slot
 */
static int get_index(struct inode *ip, u32 index,
		     dir_table_slot_t * dirtab_slot)
{
	metapage_t *mp = 0;
	dir_table_slot_t *slot;

	slot = find_index(ip, index, &mp);
	if (slot == 0) {
		return -EIO;
	}

	memcpy(dirtab_slot, slot, sizeof(dir_table_slot_t));

	if (mp)
		release_metapage(mp);

	return 0;
}

/*
 *	dtSearch()
 *
 * function:
 *	Search for the entry with specified key
 *
 * parameter:
 *
 * return: 0 - search result on stack, leaf page pinned;
 *	   errno - I/O error
 */
int dtSearch(struct inode *ip,
	 component_t * key, ino_t * data, btstack_t * btstack, int flag)
{
	int rc = 0;
	int cmp = 1;		/* init for empty page */
	s64 bn;
	metapage_t *mp;
	dtpage_t *p;
	s8 *stbl;
	int base, index, lim;
	btframe_t *btsp;
	pxd_t *pxd;
	int psize = 288;	/* initial in-line directory */
	ino_t inumber;
	component_t ciKey;
	struct super_block *sb = ip->i_sb;

	ciKey.name =
	    (wchar_t *) kmalloc((JFS_NAME_MAX + 1) * sizeof(wchar_t),
				GFP_NOFS);
	if (ciKey.name == 0) {
		rc = ENOMEM;
		goto dtSearch_Exit2;
	}


	/* uppercase search key for c-i directory */
	UniStrcpy(ciKey.name, key->name);
	ciKey.namlen = key->namlen;

	/* only uppercase if case-insensitive support is on */
	if ((JFS_SBI(sb)->mntflag & JFS_OS2) == JFS_OS2) {
		ciToUpper(&ciKey);
	}
	BT_CLR(btstack);	/* reset stack */

	/* init level count for max pages to split */
	btstack->nsplit = 1;

	/*
	 *      search down tree from root:
	 *
	 * between two consecutive entries of <Ki, Pi> and <Kj, Pj> of
	 * internal page, child page Pi contains entry with k, Ki <= K < Kj.
	 *
	 * if entry with search key K is not found
	 * internal page search find the entry with largest key Ki
	 * less than K which point to the child page to search;
	 * leaf page search find the entry with smallest key Kj
	 * greater than K so that the returned index is the position of
	 * the entry to be shifted right for insertion of new entry.
	 * for empty tree, search key is greater than any key of the tree.
	 *
	 * by convention, root bn = 0.
	 */
	for (bn = 0;;) {
		/* get/pin the page to search */
		DT_GETPAGE(ip, bn, mp, psize, p, rc);
		if (rc)
			goto dtSearch_Exit1;

		/* get sorted entry table of the page */
		stbl = DT_GETSTBL(p);

		/*
		 * binary search with search key K on the current page.
		 */
		for (base = 0, lim = p->header.nextindex; lim; lim >>= 1) {
			index = base + (lim >> 1);

			if (p->header.flag & BT_LEAF) {
				/* uppercase leaf name to compare */
				cmp =
				    ciCompare(&ciKey, p, stbl[index],
					      JFS_SBI(sb)->mntflag);
			} else {
				/* router key is in uppercase */

				cmp = dtCompare(&ciKey, p, stbl[index]);


			}
			if (cmp == 0) {
				/*
				 *      search hit
				 */
				/* search hit - leaf page:
				 * return the entry found
				 */
				if (p->header.flag & BT_LEAF) {
					inumber = le32_to_cpu(
			((ldtentry_t *) & p->slot[stbl[index]])->inumber);

					/*
					 * search for JFS_LOOKUP
					 */
					if (flag == JFS_LOOKUP) {
						*data = inumber;
						rc = 0;
						goto out;
					}

					/*
					 * search for JFS_CREATE
					 */
					if (flag == JFS_CREATE) {
						*data = inumber;
						rc = EEXIST;
						goto out;
					}

					/*
					 * search for JFS_REMOVE or JFS_RENAME
					 */
					if ((flag == JFS_REMOVE ||
					     flag == JFS_RENAME) &&
					    *data != inumber) {
						rc = ESTALE;
						goto out;
					}

					/*
					 * JFS_REMOVE|JFS_FINDDIR|JFS_RENAME
					 */
					/* save search result */
					*data = inumber;
					btsp = btstack->top;
					btsp->bn = bn;
					btsp->index = index;
					btsp->mp = mp;

					rc = 0;
					goto dtSearch_Exit1;
				}

				/* search hit - internal page:
				 * descend/search its child page
				 */
				goto getChild;
			}

			if (cmp > 0) {
				base = index + 1;
				--lim;
			}
		}

		/*
		 *      search miss
		 *
		 * base is the smallest index with key (Kj) greater than
		 * search key (K) and may be zero or (maxindex + 1) index.
		 */
		/*
		 * search miss - leaf page
		 *
		 * return location of entry (base) where new entry with
		 * search key K is to be inserted.
		 */
		if (p->header.flag & BT_LEAF) {
			/*
			 * search for JFS_LOOKUP, JFS_REMOVE, or JFS_RENAME
			 */
			if (flag == JFS_LOOKUP || flag == JFS_REMOVE ||
			    flag == JFS_RENAME) {
				rc = ENOENT;
				goto out;
			}

			/*
			 * search for JFS_CREATE|JFS_FINDDIR:
			 *
			 * save search result
			 */
			*data = 0;
			btsp = btstack->top;
			btsp->bn = bn;
			btsp->index = base;
			btsp->mp = mp;

			rc = 0;
			goto dtSearch_Exit1;
		}

		/*
		 * search miss - internal page
		 *
		 * if base is non-zero, decrement base by one to get the parent
		 * entry of the child page to search.
		 */
		index = base ? base - 1 : base;

		/*
		 * go down to child page
		 */
	      getChild:
		/* update max. number of pages to split */
		if (btstack->nsplit >= 8) {
			/* Something's corrupted, mark filesytem dirty so
			 * chkdsk will fix it.
			 */
			jERROR(1, ("stack overrun in dtSearch!\n"));
			updateSuper(sb, FM_DIRTY);
			rc = EIO;
			goto out;
		}
		btstack->nsplit++;

		/* push (bn, index) of the parent page/entry */
		BT_PUSH(btstack, bn, index);

		/* get the child page block number */
		pxd = (pxd_t *) & p->slot[stbl[index]];
		bn = addressPXD(pxd);
		psize = lengthPXD(pxd) << JFS_SBI(ip->i_sb)->l2bsize;

		/* unpin the parent page */
		DT_PUTPAGE(mp);
	}

      out:
	DT_PUTPAGE(mp);

      dtSearch_Exit1:

	kfree(ciKey.name);

      dtSearch_Exit2:

	return rc;
}


/*
 *	dtInsert()
 *
 * function: insert an entry to directory tree
 *
 * parameter:
 *
 * return: 0 - success;
 *	   errno - failure;
 */
int dtInsert(tid_t tid, struct inode *ip,
	 component_t * name, ino_t * fsn, btstack_t * btstack)
{
	int rc = 0;
	metapage_t *mp;		/* meta-page buffer */
	dtpage_t *p;		/* base B+-tree index page */
	s64 bn;
	int index;
	dtsplit_t split;	/* split information */
	ddata_t data;
	dtlock_t *dtlck;
	int n;
	tlock_t *tlck;
	lv_t *lv;

	/*
	 *      retrieve search result
	 *
	 * dtSearch() returns (leaf page pinned, index at which to insert).
	 * n.b. dtSearch() may return index of (maxindex + 1) of
	 * the full page.
	 */
	DT_GETSEARCH(ip, btstack->top, bn, mp, p, index);

	/*
	 *      insert entry for new key
	 */
	if (DO_INDEX(ip)) {
		if (JFS_IP(ip)->next_index == DIREND) {
			DT_PUTPAGE(mp);
			return EMLINK;
		}
		n = NDTLEAF(name->namlen);
		data.leaf.tid = tid;
		data.leaf.ip = ip;
	} else {
		n = NDTLEAF_LEGACY(name->namlen);
		data.leaf.ip = 0;	/* signifies legacy directory format */
	}
	data.leaf.ino = cpu_to_le32(*fsn);

	/*
	 *      leaf page does not have enough room for new entry:
	 *
	 *      extend/split the leaf page;
	 *
	 * dtSplitUp() will insert the entry and unpin the leaf page.
	 */
	if (n > p->header.freecnt) {
		split.mp = mp;
		split.index = index;
		split.nslot = n;
		split.key = name;
		split.data = &data;
		rc = dtSplitUp(tid, ip, &split, btstack);
		return rc;
	}

	/*
	 *      leaf page does have enough room for new entry:
	 *
	 *      insert the new data entry into the leaf page;
	 */
	BT_MARK_DIRTY(mp, ip);
	/*
	 * acquire a transaction lock on the leaf page
	 */
	tlck = txLock(tid, ip, mp, tlckDTREE | tlckENTRY);
	dtlck = (dtlock_t *) & tlck->lock;
	ASSERT(dtlck->index == 0);
	lv = (lv_t *) & dtlck->lv[0];

	/* linelock header */
	lv->offset = 0;
	lv->length = 1;
	dtlck->index++;

	dtInsertEntry(p, index, name, &data, &dtlck);

	/* linelock stbl of non-root leaf page */
	if (!(p->header.flag & BT_ROOT)) {
		if (dtlck->index >= dtlck->maxcnt)
			dtlck = (dtlock_t *) txLinelock(dtlck);
		lv = (lv_t *) & dtlck->lv[dtlck->index];
		n = index >> L2DTSLOTSIZE;
		lv->offset = p->header.stblindex + n;
		lv->length =
		    ((p->header.nextindex - 1) >> L2DTSLOTSIZE) - n + 1;
		dtlck->index++;
	}

	/* unpin the leaf page */
	DT_PUTPAGE(mp);

	return 0;
}


/*
 *	dtSplitUp()
 *
 * function: propagate insertion bottom up;
 *
 * parameter:
 *
 * return: 0 - success;
 *	   errno - failure;
 * 	leaf page unpinned;
 */
static int dtSplitUp(tid_t tid,
	  struct inode *ip, dtsplit_t * split, btstack_t * btstack)
{
	struct jfs_sb_info *sbi = JFS_SBI(ip->i_sb);
	int rc = 0;
	metapage_t *smp;
	dtpage_t *sp;		/* split page */
	metapage_t *rmp;
	dtpage_t *rp;		/* new right page split from sp */
	pxd_t rpxd;		/* new right page extent descriptor */
	metapage_t *lmp;
	dtpage_t *lp;		/* left child page */
	int skip;		/* index of entry of insertion */
	btframe_t *parent;	/* parent page entry on traverse stack */
	s64 xaddr, nxaddr;
	int xlen, xsize;
	pxdlist_t pxdlist;
	pxd_t *pxd;
	component_t key = { 0, 0 };
	ddata_t *data = split->data;
	int n;
	dtlock_t *dtlck;
	tlock_t *tlck;
	lv_t *lv;

	/* get split page */
	smp = split->mp;
	sp = DT_PAGE(ip, smp);

	key.name =
	    (wchar_t *) kmalloc((JFS_NAME_MAX + 2) * sizeof(wchar_t),
				GFP_NOFS);
	if (key.name == 0) {
		DT_PUTPAGE(smp);
		rc = ENOMEM;
		goto dtSplitUp_Exit;
	}

	/*
	 *      split leaf page
	 *
	 * The split routines insert the new entry, and
	 * acquire txLock as appropriate.
	 */
	/*
	 *      split root leaf page:
	 */
	if (sp->header.flag & BT_ROOT) {
		/*
		 * allocate a single extent child page
		 */
		xlen = 1;
		n = sbi->bsize >> L2DTSLOTSIZE;
		n -= (n + 31) >> L2DTSLOTSIZE;	/* stbl size */
		n -= DTROOTMAXSLOT - sp->header.freecnt; /* header + entries */
		if (n <= split->nslot)
			xlen++;
		if ((rc = dbAlloc(ip, 0, (s64) xlen, &xaddr)))
			goto freeKeyName;

		pxdlist.maxnpxd = 1;
		pxdlist.npxd = 0;
		pxd = &pxdlist.pxd[0];
		PXDaddress(pxd, xaddr);
		PXDlength(pxd, xlen);
		split->pxdlist = &pxdlist;
		rc = dtSplitRoot(tid, ip, split, &rmp);

		DT_PUTPAGE(rmp);
		DT_PUTPAGE(smp);

		goto freeKeyName;
	}

	/*
	 *      extend first leaf page
	 *
	 * extend the 1st extent if less than buffer page size
	 * (dtExtendPage() reurns leaf page unpinned)
	 */
	pxd = &sp->header.self;
	xlen = lengthPXD(pxd);
	xsize = xlen << sbi->l2bsize;
	if (xsize < PSIZE) {
		xaddr = addressPXD(pxd);
		n = xsize >> L2DTSLOTSIZE;
		n -= (n + 31) >> L2DTSLOTSIZE;	/* stbl size */
		if ((n + sp->header.freecnt) <= split->nslot)
			n = xlen + (xlen << 1);
		else
			n = xlen;
		if ((rc = dbReAlloc(sbi->ipbmap, xaddr, (s64) xlen,
				    (s64) n, &nxaddr)))
			goto extendOut;

		pxdlist.maxnpxd = 1;
		pxdlist.npxd = 0;
		pxd = &pxdlist.pxd[0];
		PXDaddress(pxd, nxaddr)
		    PXDlength(pxd, xlen + n);
		split->pxdlist = &pxdlist;
		if ((rc = dtExtendPage(tid, ip, split, btstack))) {
			nxaddr = addressPXD(pxd);
			if (xaddr != nxaddr) {
				/* free relocated extent */
				xlen = lengthPXD(pxd);
				dbFree(ip, nxaddr, (s64) xlen);
			} else {
				/* free extended delta */
				xlen = lengthPXD(pxd) - n;
				xaddr = addressPXD(pxd) + xlen;
				dbFree(ip, xaddr, (s64) n);
			}
		}

	      extendOut:
		DT_PUTPAGE(smp);
		goto freeKeyName;
	}

	/*
	 *      split leaf page <sp> into <sp> and a new right page <rp>.
	 *
	 * return <rp> pinned and its extent descriptor <rpxd>
	 */
	/*
	 * allocate new directory page extent and
	 * new index page(s) to cover page split(s)
	 *
	 * allocation hint: ?
	 */
	n = btstack->nsplit;
	pxdlist.maxnpxd = pxdlist.npxd = 0;
	xlen = sbi->nbperpage;
	for (pxd = pxdlist.pxd; n > 0; n--, pxd++) {
		if ((rc = dbAlloc(ip, 0, (s64) xlen, &xaddr)) == 0) {
			PXDaddress(pxd, xaddr);
			PXDlength(pxd, xlen);
			pxdlist.maxnpxd++;
			continue;
		}

		DT_PUTPAGE(smp);

		/* undo allocation */
		goto splitOut;
	}

	split->pxdlist = &pxdlist;
	if ((rc = dtSplitPage(tid, ip, split, &rmp, &rp, &rpxd))) {
		DT_PUTPAGE(smp);

		/* undo allocation */
		goto splitOut;
	}

	/*
	 * propagate up the router entry for the leaf page just split
	 *
	 * insert a router entry for the new page into the parent page,
	 * propagate the insert/split up the tree by walking back the stack
	 * of (bn of parent page, index of child page entry in parent page)
	 * that were traversed during the search for the page that split.
	 *
	 * the propagation of insert/split up the tree stops if the root
	 * splits or the page inserted into doesn't have to split to hold
	 * the new entry.
	 *
	 * the parent entry for the split page remains the same, and
	 * a new entry is inserted at its right with the first key and
	 * block number of the new right page.
	 *
	 * There are a maximum of 4 pages pinned at any time:
	 * two children, left parent and right parent (when the parent splits).
	 * keep the child pages pinned while working on the parent.
	 * make sure that all pins are released at exit.
	 */
	while ((parent = BT_POP(btstack)) != NULL) {
		/* parent page specified by stack frame <parent> */

		/* keep current child pages (<lp>, <rp>) pinned */
		lmp = smp;
		lp = sp;

		/*
		 * insert router entry in parent for new right child page <rp>
		 */
		/* get the parent page <sp> */
		DT_GETPAGE(ip, parent->bn, smp, PSIZE, sp, rc);
		if (rc) {
			DT_PUTPAGE(lmp);
			DT_PUTPAGE(rmp);
			goto splitOut;
		}

		/*
		 * The new key entry goes ONE AFTER the index of parent entry,
		 * because the split was to the right.
		 */
		skip = parent->index + 1;

		/*
		 * compute the key for the router entry
		 *
		 * key suffix compression:
		 * for internal pages that have leaf pages as children,
		 * retain only what's needed to distinguish between
		 * the new entry and the entry on the page to its left.
		 * If the keys compare equal, retain the entire key.
		 *
		 * note that compression is performed only at computing
		 * router key at the lowest internal level.
		 * further compression of the key between pairs of higher
		 * level internal pages loses too much information and
		 * the search may fail.
		 * (e.g., two adjacent leaf pages of {a, ..., x} {xx, ...,}
		 * results in two adjacent parent entries (a)(xx).
		 * if split occurs between these two entries, and
		 * if compression is applied, the router key of parent entry
		 * of right page (x) will divert search for x into right
		 * subtree and miss x in the left subtree.)
		 *
		 * the entire key must be retained for the next-to-leftmost
		 * internal key at any level of the tree, or search may fail
		 * (e.g., ?)
		 */
		switch (rp->header.flag & BT_TYPE) {
		case BT_LEAF:
			/*
			 * compute the length of prefix for suffix compression
			 * between last entry of left page and first entry
			 * of right page
			 */
			if ((sp->header.flag & BT_ROOT && skip > 1) ||
			    sp->header.prev != 0 || skip > 1) {
				/* compute uppercase router prefix key */
				ciGetLeafPrefixKey(lp,
						   lp->header.nextindex - 1,
						   rp, 0, &key, sbi->mntflag);
			} else {
				/* next to leftmost entry of
				   lowest internal level */

				/* compute uppercase router key */
				dtGetKey(rp, 0, &key, sbi->mntflag);
				key.name[key.namlen] = 0;

				if ((sbi->mntflag & JFS_OS2) == JFS_OS2)
					ciToUpper(&key);
			}

			n = NDTINTERNAL(key.namlen);
			break;

		case BT_INTERNAL:
			dtGetKey(rp, 0, &key, sbi->mntflag);
			n = NDTINTERNAL(key.namlen);
			break;

		default:
			jERROR(2, ("dtSplitUp(): UFO!\n"));
			break;
		}

		/* unpin left child page */
		DT_PUTPAGE(lmp);

		/*
		 * compute the data for the router entry
		 */
		data->xd = rpxd;	/* child page xd */

		/*
		 * parent page is full - split the parent page
		 */
		if (n > sp->header.freecnt) {
			/* init for parent page split */
			split->mp = smp;
			split->index = skip;	/* index at insert */
			split->nslot = n;
			split->key = &key;
			/* split->data = data; */

			/* unpin right child page */
			DT_PUTPAGE(rmp);

			/* The split routines insert the new entry,
			 * acquire txLock as appropriate.
			 * return <rp> pinned and its block number <rbn>.
			 */
			rc = (sp->header.flag & BT_ROOT) ?
			    dtSplitRoot(tid, ip, split, &rmp) :
			    dtSplitPage(tid, ip, split, &rmp, &rp, &rpxd);
			if (rc) {
				DT_PUTPAGE(smp);
				goto splitOut;
			}

			/* smp and rmp are pinned */
		}
		/*
		 * parent page is not full - insert router entry in parent page
		 */
		else {
			BT_MARK_DIRTY(smp, ip);
			/*
			 * acquire a transaction lock on the parent page
			 */
			tlck = txLock(tid, ip, smp, tlckDTREE | tlckENTRY);
			dtlck = (dtlock_t *) & tlck->lock;
			ASSERT(dtlck->index == 0);
			lv = (lv_t *) & dtlck->lv[0];

			/* linelock header */
			lv->offset = 0;
			lv->length = 1;
			dtlck->index++;

			/* linelock stbl of non-root parent page */
			if (!(sp->header.flag & BT_ROOT)) {
				lv++;
				n = skip >> L2DTSLOTSIZE;
				lv->offset = sp->header.stblindex + n;
				lv->length =
				    ((sp->header.nextindex -
				      1) >> L2DTSLOTSIZE) - n + 1;
				dtlck->index++;
			}

			dtInsertEntry(sp, skip, &key, data, &dtlck);

			/* exit propagate up */
			break;
		}
	}

	/* unpin current split and its right page */
	DT_PUTPAGE(smp);
	DT_PUTPAGE(rmp);

	/*
	 * free remaining extents allocated for split
	 */
      splitOut:
	n = pxdlist.npxd;
	pxd = &pxdlist.pxd[n];
	for (; n < pxdlist.maxnpxd; n++, pxd++)
		dbFree(ip, addressPXD(pxd), (s64) lengthPXD(pxd));

      freeKeyName:
	kfree(key.name);

      dtSplitUp_Exit:

	return rc;
}


/*
 *	dtSplitPage()
 *
 * function: Split a non-root page of a btree.
 *
 * parameter:
 *
 * return: 0 - success;
 *	   errno - failure;
 *	return split and new page pinned;
 */
static int dtSplitPage(tid_t tid, struct inode *ip, dtsplit_t * split,
	    metapage_t ** rmpp, dtpage_t ** rpp, pxd_t * rpxdp)
{
	struct super_block *sb = ip->i_sb;
	int rc = 0;
	metapage_t *smp;
	dtpage_t *sp;
	metapage_t *rmp;
	dtpage_t *rp;		/* new right page allocated */
	s64 rbn;		/* new right page block number */
	metapage_t *mp;
	dtpage_t *p;
	s64 nextbn;
	pxdlist_t *pxdlist;
	pxd_t *pxd;
	int skip, nextindex, half, left, nxt, off, si;
	ldtentry_t *ldtentry;
	idtentry_t *idtentry;
	u8 *stbl;
	dtslot_t *f;
	int fsi, stblsize;
	int n;
	dtlock_t *sdtlck, *rdtlck;
	tlock_t *tlck;
	dtlock_t *dtlck;
	lv_t *slv, *rlv, *lv;

	/* get split page */
	smp = split->mp;
	sp = DT_PAGE(ip, smp);

	/*
	 * allocate the new right page for the split
	 */
	pxdlist = split->pxdlist;
	pxd = &pxdlist->pxd[pxdlist->npxd];
	pxdlist->npxd++;
	rbn = addressPXD(pxd);
	rmp = get_metapage(ip, rbn, PSIZE, 1);
	if (rmp == NULL)
		return EIO;

	jEVENT(0,
	       ("dtSplitPage: ip:0x%p smp:0x%p rmp:0x%p\n", ip, smp, rmp));

	BT_MARK_DIRTY(rmp, ip);
	/*
	 * acquire a transaction lock on the new right page
	 */
	tlck = txLock(tid, ip, rmp, tlckDTREE | tlckNEW);
	rdtlck = (dtlock_t *) & tlck->lock;

	rp = (dtpage_t *) rmp->data;
	*rpp = rp;
	rp->header.self = *pxd;

	BT_MARK_DIRTY(smp, ip);
	/*
	 * acquire a transaction lock on the split page
	 *
	 * action:
	 */
	tlck = txLock(tid, ip, smp, tlckDTREE | tlckENTRY);
	sdtlck = (dtlock_t *) & tlck->lock;

	/* linelock header of split page */
	ASSERT(sdtlck->index == 0);
	slv = (lv_t *) & sdtlck->lv[0];
	slv->offset = 0;
	slv->length = 1;
	sdtlck->index++;

	/*
	 * initialize/update sibling pointers between sp and rp
	 */
	nextbn = le64_to_cpu(sp->header.next);
	rp->header.next = cpu_to_le64(nextbn);
	rp->header.prev = cpu_to_le64(addressPXD(&sp->header.self));
	sp->header.next = cpu_to_le64(rbn);

	/*
	 * initialize new right page
	 */
	rp->header.flag = sp->header.flag;

	/* compute sorted entry table at start of extent data area */
	rp->header.nextindex = 0;
	rp->header.stblindex = 1;

	n = PSIZE >> L2DTSLOTSIZE;
	rp->header.maxslot = n;
	stblsize = (n + 31) >> L2DTSLOTSIZE;	/* in unit of slot */

	/* init freelist */
	fsi = rp->header.stblindex + stblsize;
	rp->header.freelist = fsi;
	rp->header.freecnt = rp->header.maxslot - fsi;

	/*
	 *      sequential append at tail: append without split
	 *
	 * If splitting the last page on a level because of appending
	 * a entry to it (skip is maxentry), it's likely that the access is
	 * sequential. Adding an empty page on the side of the level is less
	 * work and can push the fill factor much higher than normal.
	 * If we're wrong it's no big deal, we'll just do the split the right
	 * way next time.
	 * (It may look like it's equally easy to do a similar hack for
	 * reverse sorted data, that is, split the tree left,
	 * but it's not. Be my guest.)
	 */
	if (nextbn == 0 && split->index == sp->header.nextindex) {
		/* linelock header + stbl (first slot) of new page */
		rlv = (lv_t *) & rdtlck->lv[rdtlck->index];
		rlv->offset = 0;
		rlv->length = 2;
		rdtlck->index++;

		/*
		 * initialize freelist of new right page
		 */
		f = &rp->slot[fsi];
		for (fsi++; fsi < rp->header.maxslot; f++, fsi++)
			f->next = fsi;
		f->next = -1;

		/* insert entry at the first entry of the new right page */
		dtInsertEntry(rp, 0, split->key, split->data, &rdtlck);

		goto out;
	}

	/*
	 *      non-sequential insert (at possibly middle page)
	 */

	/*
	 * update prev pointer of previous right sibling page;
	 */
	if (nextbn != 0) {
		DT_GETPAGE(ip, nextbn, mp, PSIZE, p, rc);
		if (rc)
			return rc;

		BT_MARK_DIRTY(mp, ip);
		/*
		 * acquire a transaction lock on the next page
		 */
		tlck = txLock(tid, ip, mp, tlckDTREE | tlckRELINK);
		jEVENT(0,
		       ("dtSplitPage: tlck = 0x%p, ip = 0x%p, mp=0x%p\n",
			tlck, ip, mp));
		dtlck = (dtlock_t *) & tlck->lock;

		/* linelock header of previous right sibling page */
		lv = (lv_t *) & dtlck->lv[dtlck->index];
		lv->offset = 0;
		lv->length = 1;
		dtlck->index++;

		p->header.prev = cpu_to_le64(rbn);

		DT_PUTPAGE(mp);
	}

	/*
	 * split the data between the split and right pages.
	 */
	skip = split->index;
	half = (PSIZE >> L2DTSLOTSIZE) >> 1;	/* swag */
	left = 0;

	/*
	 *      compute fill factor for split pages
	 *
	 * <nxt> traces the next entry to move to rp
	 * <off> traces the next entry to stay in sp
	 */
	stbl = (u8 *) & sp->slot[sp->header.stblindex];
	nextindex = sp->header.nextindex;
	for (nxt = off = 0; nxt < nextindex; ++off) {
		if (off == skip)
			/* check for fill factor with new entry size */
			n = split->nslot;
		else {
			si = stbl[nxt];
			switch (sp->header.flag & BT_TYPE) {
			case BT_LEAF:
				ldtentry = (ldtentry_t *) & sp->slot[si];
				if (DO_INDEX(ip))
					n = NDTLEAF(ldtentry->namlen);
				else
					n = NDTLEAF_LEGACY(ldtentry->
							   namlen);
				break;

			case BT_INTERNAL:
				idtentry = (idtentry_t *) & sp->slot[si];
				n = NDTINTERNAL(idtentry->namlen);
				break;

			default:
				break;
			}

			++nxt;	/* advance to next entry to move in sp */
		}

		left += n;
		if (left >= half)
			break;
	}

	/* <nxt> poins to the 1st entry to move */

	/*
	 *      move entries to right page
	 *
	 * dtMoveEntry() initializes rp and reserves entry for insertion
	 *
	 * split page moved out entries are linelocked;
	 * new/right page moved in entries are linelocked;
	 */
	/* linelock header + stbl of new right page */
	rlv = (lv_t *) & rdtlck->lv[rdtlck->index];
	rlv->offset = 0;
	rlv->length = 5;
	rdtlck->index++;

	dtMoveEntry(sp, nxt, rp, &sdtlck, &rdtlck, DO_INDEX(ip));

	sp->header.nextindex = nxt;

	/*
	 * finalize freelist of new right page
	 */
	fsi = rp->header.freelist;
	f = &rp->slot[fsi];
	for (fsi++; fsi < rp->header.maxslot; f++, fsi++)
		f->next = fsi;
	f->next = -1;

	/*
	 * Update directory index table for entries now in right page
	 */
	if ((rp->header.flag & BT_LEAF) && DO_INDEX(ip)) {
		mp = 0;
		stbl = DT_GETSTBL(rp);
		for (n = 0; n < rp->header.nextindex; n++) {
			ldtentry = (ldtentry_t *) & rp->slot[stbl[n]];
			modify_index(tid, ip, le32_to_cpu(ldtentry->index),
				     rbn, n, &mp);
		}
		if (mp)
			release_metapage(mp);
	}

	/*
	 * the skipped index was on the left page,
	 */
	if (skip <= off) {
		/* insert the new entry in the split page */
		dtInsertEntry(sp, skip, split->key, split->data, &sdtlck);

		/* linelock stbl of split page */
		if (sdtlck->index >= sdtlck->maxcnt)
			sdtlck = (dtlock_t *) txLinelock(sdtlck);
		slv = (lv_t *) & sdtlck->lv[sdtlck->index];
		n = skip >> L2DTSLOTSIZE;
		slv->offset = sp->header.stblindex + n;
		slv->length =
		    ((sp->header.nextindex - 1) >> L2DTSLOTSIZE) - n + 1;
		sdtlck->index++;
	}
	/*
	 * the skipped index was on the right page,
	 */
	else {
		/* adjust the skip index to reflect the new position */
		skip -= nxt;

		/* insert the new entry in the right page */
		dtInsertEntry(rp, skip, split->key, split->data, &rdtlck);
	}

      out:
	*rmpp = rmp;
	*rpxdp = *pxd;

	ip->i_blocks += LBLK2PBLK(sb, lengthPXD(pxd));

	jEVENT(0, ("dtSplitPage: ip:0x%p sp:0x%p rp:0x%p\n", ip, sp, rp));
	return 0;
}


/*
 *	dtExtendPage()
 *
 * function: extend 1st/only directory leaf page
 *
 * parameter:
 *
 * return: 0 - success;
 *	   errno - failure;
 *	return extended page pinned;
 */
static int dtExtendPage(tid_t tid,
	     struct inode *ip, dtsplit_t * split, btstack_t * btstack)
{
	struct super_block *sb = ip->i_sb;
	int rc;
	metapage_t *smp, *pmp, *mp;
	dtpage_t *sp, *pp;
	pxdlist_t *pxdlist;
	pxd_t *pxd, *tpxd;
	int xlen, xsize;
	int newstblindex, newstblsize;
	int oldstblindex, oldstblsize;
	int fsi, last;
	dtslot_t *f;
	btframe_t *parent;
	int n;
	dtlock_t *dtlck;
	s64 xaddr, txaddr;
	tlock_t *tlck;
	pxdlock_t *pxdlock;
	lv_t *lv;
	uint type;
	ldtentry_t *ldtentry;
	u8 *stbl;

	/* get page to extend */
	smp = split->mp;
	sp = DT_PAGE(ip, smp);

	/* get parent/root page */
	parent = BT_POP(btstack);
	DT_GETPAGE(ip, parent->bn, pmp, PSIZE, pp, rc);
	if (rc)
		return (rc);

	/*
	 *      extend the extent
	 */
	pxdlist = split->pxdlist;
	pxd = &pxdlist->pxd[pxdlist->npxd];
	pxdlist->npxd++;

	xaddr = addressPXD(pxd);
	tpxd = &sp->header.self;
	txaddr = addressPXD(tpxd);
	/* in-place extension */
	if (xaddr == txaddr) {
		type = tlckEXTEND;
	}
	/* relocation */
	else {
		type = tlckNEW;

		/* save moved extent descriptor for later free */
		tlck = txMaplock(tid, ip, tlckDTREE | tlckRELOCATE);
		pxdlock = (pxdlock_t *) & tlck->lock;
		pxdlock->flag = mlckFREEPXD;
		pxdlock->pxd = sp->header.self;
		pxdlock->index = 1;

		/*
		 * Update directory index table to reflect new page address
		 */
		if (DO_INDEX(ip)) {
			mp = 0;
			stbl = DT_GETSTBL(sp);
			for (n = 0; n < sp->header.nextindex; n++) {
				ldtentry =
				    (ldtentry_t *) & sp->slot[stbl[n]];
				modify_index(tid, ip,
					     le32_to_cpu(ldtentry->index),
					     xaddr, n, &mp);
			}
			if (mp)
				release_metapage(mp);
		}
	}

	/*
	 *      extend the page
	 */
	sp->header.self = *pxd;

	jEVENT(0,
	       ("dtExtendPage: ip:0x%p smp:0x%p sp:0x%p\n", ip, smp, sp));

	BT_MARK_DIRTY(smp, ip);
	/*
	 * acquire a transaction lock on the extended/leaf page
	 */
	tlck = txLock(tid, ip, smp, tlckDTREE | type);
	dtlck = (dtlock_t *) & tlck->lock;
	lv = (lv_t *) & dtlck->lv[0];

	/* update buffer extent descriptor of extended page */
	xlen = lengthPXD(pxd);
	xsize = xlen << JFS_SBI(sb)->l2bsize;
#ifdef _STILL_TO_PORT
	bmSetXD(smp, xaddr, xsize);
#endif				/*  _STILL_TO_PORT */

	/*
	 * copy old stbl to new stbl at start of extended area
	 */
	oldstblindex = sp->header.stblindex;
	oldstblsize = (sp->header.maxslot + 31) >> L2DTSLOTSIZE;
	newstblindex = sp->header.maxslot;
	n = xsize >> L2DTSLOTSIZE;
	newstblsize = (n + 31) >> L2DTSLOTSIZE;
	memcpy(&sp->slot[newstblindex], &sp->slot[oldstblindex],
	       sp->header.nextindex);

	/*
	 * in-line extension: linelock old area of extended page
	 */
	if (type == tlckEXTEND) {
		/* linelock header */
		lv->offset = 0;
		lv->length = 1;
		dtlck->index++;
		lv++;

		/* linelock new stbl of extended page */
		lv->offset = newstblindex;
		lv->length = newstblsize;
	}
	/*
	 * relocation: linelock whole relocated area
	 */
	else {
		lv->offset = 0;
		lv->length = sp->header.maxslot + newstblsize;
	}

	dtlck->index++;

	sp->header.maxslot = n;
	sp->header.stblindex = newstblindex;
	/* sp->header.nextindex remains the same */

	/*
	 * add old stbl region at head of freelist
	 */
	fsi = oldstblindex;
	f = &sp->slot[fsi];
	last = sp->header.freelist;
	for (n = 0; n < oldstblsize; n++, fsi++, f++) {
		f->next = last;
		last = fsi;
	}
	sp->header.freelist = last;
	sp->header.freecnt += oldstblsize;

	/*
	 * append free region of newly extended area at tail of freelist
	 */
	/* init free region of newly extended area */
	fsi = n = newstblindex + newstblsize;
	f = &sp->slot[fsi];
	for (fsi++; fsi < sp->header.maxslot; f++, fsi++)
		f->next = fsi;
	f->next = -1;

	/* append new free region at tail of old freelist */
	fsi = sp->header.freelist;
	if (fsi == -1)
		sp->header.freelist = n;
	else {
		do {
			f = &sp->slot[fsi];
			fsi = f->next;
		} while (fsi != -1);

		f->next = n;
	}

	sp->header.freecnt += sp->header.maxslot - n;

	/*
	 * insert the new entry
	 */
	dtInsertEntry(sp, split->index, split->key, split->data, &dtlck);

	BT_MARK_DIRTY(pmp, ip);
	/*
	 * linelock any freeslots residing in old extent
	 */
	if (type == tlckEXTEND) {
		n = sp->header.maxslot >> 2;
		if (sp->header.freelist < n)
			dtLinelockFreelist(sp, n, &dtlck);
	}

	/*
	 *      update parent entry on the parent/root page
	 */
	/*
	 * acquire a transaction lock on the parent/root page
	 */
	tlck = txLock(tid, ip, pmp, tlckDTREE | tlckENTRY);
	dtlck = (dtlock_t *) & tlck->lock;
	lv = (lv_t *) & dtlck->lv[dtlck->index];

	/* linelock parent entry - 1st slot */
	lv->offset = 1;
	lv->length = 1;
	dtlck->index++;

	/* update the parent pxd for page extension */
	tpxd = (pxd_t *) & pp->slot[1];
	*tpxd = *pxd;

	/* Since the directory might have an EA and/or ACL associated with it
	 * we need to make sure we take that into account when setting the
	 * i_nblocks
	 */
	ip->i_blocks = LBLK2PBLK(ip->i_sb, xlen +
				 ((JFS_IP(ip)->ea.flag & DXD_EXTENT) ?
				  lengthDXD(&JFS_IP(ip)->ea) : 0) +
				 ((JFS_IP(ip)->acl.flag & DXD_EXTENT) ?
				  lengthDXD(&JFS_IP(ip)->acl) : 0));

	jEVENT(0,
	       ("dtExtendPage: ip:0x%p smp:0x%p sp:0x%p\n", ip, smp, sp));


	DT_PUTPAGE(pmp);
	return 0;
}


/*
 *	dtSplitRoot()
 *
 * function:
 *	split the full root page into
 *	original/root/split page and new right page
 *	i.e., root remains fixed in tree anchor (inode) and
 *	the root is copied to a single new right child page
 *	since root page << non-root page, and
 *	the split root page contains a single entry for the
 *	new right child page.
 *
 * parameter:
 *
 * return: 0 - success;
 *	   errno - failure;
 *	return new page pinned;
 */
static int dtSplitRoot(tid_t tid,
	    struct inode *ip, dtsplit_t * split, metapage_t ** rmpp)
{
	struct super_block *sb = ip->i_sb;
	metapage_t *smp;
	dtroot_t *sp;
	metapage_t *rmp;
	dtpage_t *rp;
	s64 rbn;
	int xlen;
	int xsize;
	dtslot_t *f;
	s8 *stbl;
	int fsi, stblsize, n;
	idtentry_t *s;
	pxd_t *ppxd;
	pxdlist_t *pxdlist;
	pxd_t *pxd;
	dtlock_t *dtlck;
	tlock_t *tlck;
	lv_t *lv;

	/* get split root page */
	smp = split->mp;
	sp = &JFS_IP(ip)->i_dtroot;

	/*
	 *      allocate/initialize a single (right) child page
	 *
	 * N.B. at first split, a one (or two) block to fit new entry
	 * is allocated; at subsequent split, a full page is allocated;
	 */
	pxdlist = split->pxdlist;
	pxd = &pxdlist->pxd[pxdlist->npxd];
	pxdlist->npxd++;
	rbn = addressPXD(pxd);
	xlen = lengthPXD(pxd);
	xsize = xlen << JFS_SBI(sb)->l2bsize;
	rmp = get_metapage(ip, rbn, xsize, 1);
	rp = rmp->data;

	BT_MARK_DIRTY(rmp, ip);
	/*
	 * acquire a transaction lock on the new right page
	 */
	tlck = txLock(tid, ip, rmp, tlckDTREE | tlckNEW);
	dtlck = (dtlock_t *) & tlck->lock;

	rp->header.flag =
	    (sp->header.flag & BT_LEAF) ? BT_LEAF : BT_INTERNAL;
	rp->header.self = *pxd;

	/* initialize sibling pointers */
	rp->header.next = 0;
	rp->header.prev = 0;

	/*
	 *      move in-line root page into new right page extent
	 */
	/* linelock header + copied entries + new stbl (1st slot) in new page */
	ASSERT(dtlck->index == 0);
	lv = (lv_t *) & dtlck->lv[0];
	lv->offset = 0;
	lv->length = 10;	/* 1 + 8 + 1 */
	dtlck->index++;

	n = xsize >> L2DTSLOTSIZE;
	rp->header.maxslot = n;
	stblsize = (n + 31) >> L2DTSLOTSIZE;

	/* copy old stbl to new stbl at start of extended area */
	rp->header.stblindex = DTROOTMAXSLOT;
	stbl = (s8 *) & rp->slot[DTROOTMAXSLOT];
	memcpy(stbl, sp->header.stbl, sp->header.nextindex);
	rp->header.nextindex = sp->header.nextindex;

	/* copy old data area to start of new data area */
	memcpy(&rp->slot[1], &sp->slot[1], IDATASIZE);

	/*
	 * append free region of newly extended area at tail of freelist
	 */
	/* init free region of newly extended area */
	fsi = n = DTROOTMAXSLOT + stblsize;
	f = &rp->slot[fsi];
	for (fsi++; fsi < rp->header.maxslot; f++, fsi++)
		f->next = fsi;
	f->next = -1;

	/* append new free region at tail of old freelist */
	fsi = sp->header.freelist;
	if (fsi == -1)
		rp->header.freelist = n;
	else {
		rp->header.freelist = fsi;

		do {
			f = &rp->slot[fsi];
			fsi = f->next;
		} while (fsi != -1);

		f->next = n;
	}

	rp->header.freecnt = sp->header.freecnt + rp->header.maxslot - n;

	/*
	 * Update directory index table for entries now in right page
	 */
	if ((rp->header.flag & BT_LEAF) && DO_INDEX(ip)) {
		metapage_t *mp = 0;
		ldtentry_t *ldtentry;

		stbl = DT_GETSTBL(rp);
		for (n = 0; n < rp->header.nextindex; n++) {
			ldtentry = (ldtentry_t *) & rp->slot[stbl[n]];
			modify_index(tid, ip, le32_to_cpu(ldtentry->index),
				     rbn, n, &mp);
		}
		if (mp)
			release_metapage(mp);
	}
	/*
	 * insert the new entry into the new right/child page
	 * (skip index in the new right page will not change)
	 */
	dtInsertEntry(rp, split->index, split->key, split->data, &dtlck);

	/*
	 *      reset parent/root page
	 *
	 * set the 1st entry offset to 0, which force the left-most key
	 * at any level of the tree to be less than any search key.
	 *
	 * The btree comparison code guarantees that the left-most key on any
	 * level of the tree is never used, so it doesn't need to be filled in.
	 */
	BT_MARK_DIRTY(smp, ip);
	/*
	 * acquire a transaction lock on the root page (in-memory inode)
	 */
	tlck = txLock(tid, ip, smp, tlckDTREE | tlckNEW | tlckBTROOT);
	dtlck = (dtlock_t *) & tlck->lock;

	/* linelock root */
	ASSERT(dtlck->index == 0);
	lv = (lv_t *) & dtlck->lv[0];
	lv->offset = 0;
	lv->length = DTROOTMAXSLOT;
	dtlck->index++;

	/* update page header of root */
	if (sp->header.flag & BT_LEAF) {
		sp->header.flag &= ~BT_LEAF;
		sp->header.flag |= BT_INTERNAL;
	}

	/* init the first entry */
	s = (idtentry_t *) & sp->slot[DTENTRYSTART];
	ppxd = (pxd_t *) s;
	*ppxd = *pxd;
	s->next = -1;
	s->namlen = 0;

	stbl = sp->header.stbl;
	stbl[0] = DTENTRYSTART;
	sp->header.nextindex = 1;

	/* init freelist */
	fsi = DTENTRYSTART + 1;
	f = &sp->slot[fsi];

	/* init free region of remaining area */
	for (fsi++; fsi < DTROOTMAXSLOT; f++, fsi++)
		f->next = fsi;
	f->next = -1;

	sp->header.freelist = DTENTRYSTART + 1;
	sp->header.freecnt = DTROOTMAXSLOT - (DTENTRYSTART + 1);

	*rmpp = rmp;

	ip->i_blocks += LBLK2PBLK(ip->i_sb, lengthPXD(pxd));
	return 0;
}


/*
 *	dtDelete()
 *
 * function: delete the entry(s) referenced by a key.
 *
 * parameter:
 *
 * return:
 */
int dtDelete(tid_t tid,
	 struct inode *ip, component_t * key, ino_t * ino, int flag)
{
	int rc = 0;
	s64 bn;
	metapage_t *mp, *imp;
	dtpage_t *p;
	int index;
	btstack_t btstack;
	dtlock_t *dtlck;
	tlock_t *tlck;
	lv_t *lv;
	int i;
	ldtentry_t *ldtentry;
	u8 *stbl;
	u32 table_index, next_index;
	metapage_t *nmp;
	dtpage_t *np;

	/*
	 *      search for the entry to delete:
	 *
	 * dtSearch() returns (leaf page pinned, index at which to delete).
	 */
	if ((rc = dtSearch(ip, key, ino, &btstack, flag)))
		return rc;

	/* retrieve search result */
	DT_GETSEARCH(ip, btstack.top, bn, mp, p, index);

	/*
	 * We need to find put the index of the next entry into the
	 * directory index table in order to resume a readdir from this
	 * entry.
	 */
	if (DO_INDEX(ip)) {
		stbl = DT_GETSTBL(p);
		ldtentry = (ldtentry_t *) & p->slot[stbl[index]];
		table_index = le32_to_cpu(ldtentry->index);
		if (index == (p->header.nextindex - 1)) {
			/*
			 * Last entry in this leaf page
			 */
			if ((p->header.flag & BT_ROOT)
			    || (p->header.next == 0))
				next_index = -1;
			else {
				/* Read next leaf page */
				DT_GETPAGE(ip, le64_to_cpu(p->header.next),
					   nmp, PSIZE, np, rc);
				if (rc)
					next_index = -1;
				else {
					stbl = DT_GETSTBL(np);
					ldtentry =
					    (ldtentry_t *) & np->
					    slot[stbl[0]];
					next_index =
					    le32_to_cpu(ldtentry->index);
					DT_PUTPAGE(nmp);
				}
			}
		} else {
			ldtentry =
			    (ldtentry_t *) & p->slot[stbl[index + 1]];
			next_index = le32_to_cpu(ldtentry->index);
		}
		free_index(tid, ip, table_index, next_index);
	}
	/*
	 * the leaf page becomes empty, delete the page
	 */
	if (p->header.nextindex == 1) {
		/* delete empty page */
		rc = dtDeleteUp(tid, ip, mp, p, &btstack);
	}
	/*
	 * the leaf page has other entries remaining:
	 *
	 * delete the entry from the leaf page.
	 */
	else {
		BT_MARK_DIRTY(mp, ip);
		/*
		 * acquire a transaction lock on the leaf page
		 */
		tlck = txLock(tid, ip, mp, tlckDTREE | tlckENTRY);
		dtlck = (dtlock_t *) & tlck->lock;

		/*
		 * Do not assume that dtlck->index will be zero.  During a
		 * rename within a directory, this transaction may have
		 * modified this page already when adding the new entry.
		 */

		/* linelock header */
		if (dtlck->index >= dtlck->maxcnt)
			dtlck = (dtlock_t *) txLinelock(dtlck);
		lv = (lv_t *) & dtlck->lv[dtlck->index];
		lv->offset = 0;
		lv->length = 1;
		dtlck->index++;

		/* linelock stbl of non-root leaf page */
		if (!(p->header.flag & BT_ROOT)) {
			if (dtlck->index >= dtlck->maxcnt)
				dtlck = (dtlock_t *) txLinelock(dtlck);
			lv = (lv_t *) & dtlck->lv[dtlck->index];
			i = index >> L2DTSLOTSIZE;
			lv->offset = p->header.stblindex + i;
			lv->length =
			    ((p->header.nextindex - 1) >> L2DTSLOTSIZE) -
			    i + 1;
			dtlck->index++;
		}

		/* free the leaf entry */
		dtDeleteEntry(p, index, &dtlck);

		/*
		 * Update directory index table for entries moved in stbl
		 */
		if (DO_INDEX(ip) && index < p->header.nextindex) {
			imp = 0;
			stbl = DT_GETSTBL(p);
			for (i = index; i < p->header.nextindex; i++) {
				ldtentry =
				    (ldtentry_t *) & p->slot[stbl[i]];
				modify_index(tid, ip,
					     le32_to_cpu(ldtentry->index),
					     bn, i, &imp);
			}
			if (imp)
				release_metapage(imp);
		}

		DT_PUTPAGE(mp);
	}

	return rc;
}


/*
 *	dtDeleteUp()
 *
 * function:
 *	free empty pages as propagating deletion up the tree
 *
 * parameter:
 *
 * return:
 */
static int dtDeleteUp(tid_t tid, struct inode *ip,
	   metapage_t * fmp, dtpage_t * fp, btstack_t * btstack)
{
	int rc = 0;
	metapage_t *mp;
	dtpage_t *p;
	int index, nextindex;
	int xlen;
	btframe_t *parent;
	dtlock_t *dtlck;
	tlock_t *tlck;
	lv_t *lv;
	pxdlock_t *pxdlock;
	int i;

	/*
	 *      keep the root leaf page which has become empty
	 */
	if (BT_IS_ROOT(fmp)) {
		/*
		 * reset the root
		 *
		 * dtInitRoot() acquires txlock on the root
		 */
		dtInitRoot(tid, ip, PARENT(ip));

		DT_PUTPAGE(fmp);

		return 0;
	}

	/*
	 *      free the non-root leaf page
	 */
	/*
	 * acquire a transaction lock on the page
	 *
	 * write FREEXTENT|NOREDOPAGE log record
	 * N.B. linelock is overlaid as freed extent descriptor, and
	 * the buffer page is freed;
	 */
	tlck = txMaplock(tid, ip, tlckDTREE | tlckFREE);
	pxdlock = (pxdlock_t *) & tlck->lock;
	pxdlock->flag = mlckFREEPXD;
	pxdlock->pxd = fp->header.self;
	pxdlock->index = 1;

	/* update sibling pointers */
	if ((rc = dtRelink(tid, ip, fp)))
		return rc;

	xlen = lengthPXD(&fp->header.self);
	ip->i_blocks -= LBLK2PBLK(ip->i_sb, xlen);

	/* free/invalidate its buffer page */
	discard_metapage(fmp);

	/*
	 *      propagate page deletion up the directory tree
	 *
	 * If the delete from the parent page makes it empty,
	 * continue all the way up the tree.
	 * stop if the root page is reached (which is never deleted) or
	 * if the entry deletion does not empty the page.
	 */
	while ((parent = BT_POP(btstack)) != NULL) {
		/* pin the parent page <sp> */
		DT_GETPAGE(ip, parent->bn, mp, PSIZE, p, rc);
		if (rc)
			return rc;

		/*
		 * free the extent of the child page deleted
		 */
		index = parent->index;

		/*
		 * delete the entry for the child page from parent
		 */
		nextindex = p->header.nextindex;

		/*
		 * the parent has the single entry being deleted:
		 *
		 * free the parent page which has become empty.
		 */
		if (nextindex == 1) {
			/*
			 * keep the root internal page which has become empty
			 */
			if (p->header.flag & BT_ROOT) {
				/*
				 * reset the root
				 *
				 * dtInitRoot() acquires txlock on the root
				 */
				dtInitRoot(tid, ip, PARENT(ip));

				DT_PUTPAGE(mp);

				return 0;
			}
			/*
			 * free the parent page
			 */
			else {
				/*
				 * acquire a transaction lock on the page
				 *
				 * write FREEXTENT|NOREDOPAGE log record
				 */
				tlck =
				    txMaplock(tid, ip,
					      tlckDTREE | tlckFREE);
				pxdlock = (pxdlock_t *) & tlck->lock;
				pxdlock->flag = mlckFREEPXD;
				pxdlock->pxd = p->header.self;
				pxdlock->index = 1;

				/* update sibling pointers */
				if ((rc = dtRelink(tid, ip, p)))
					return rc;

				xlen = lengthPXD(&p->header.self);
				ip->i_blocks -= LBLK2PBLK(ip->i_sb, xlen);

				/* free/invalidate its buffer page */
				discard_metapage(mp);

				/* propagate up */
				continue;
			}
		}

		/*
		 * the parent has other entries remaining:
		 *
		 * delete the router entry from the parent page.
		 */
		BT_MARK_DIRTY(mp, ip);
		/*
		 * acquire a transaction lock on the page
		 *
		 * action: router entry deletion
		 */
		tlck = txLock(tid, ip, mp, tlckDTREE | tlckENTRY);
		dtlck = (dtlock_t *) & tlck->lock;

		/* linelock header */
		if (dtlck->index >= dtlck->maxcnt)
			dtlck = (dtlock_t *) txLinelock(dtlck);
		lv = (lv_t *) & dtlck->lv[dtlck->index];
		lv->offset = 0;
		lv->length = 1;
		dtlck->index++;

		/* linelock stbl of non-root leaf page */
		if (!(p->header.flag & BT_ROOT)) {
			if (dtlck->index < dtlck->maxcnt)
				lv++;
			else {
				dtlck = (dtlock_t *) txLinelock(dtlck);
				lv = (lv_t *) & dtlck->lv[0];
			}
			i = index >> L2DTSLOTSIZE;
			lv->offset = p->header.stblindex + i;
			lv->length =
			    ((p->header.nextindex - 1) >> L2DTSLOTSIZE) -
			    i + 1;
			dtlck->index++;
		}

		/* free the router entry */
		dtDeleteEntry(p, index, &dtlck);

		/* reset key of new leftmost entry of level (for consistency) */
		if (index == 0 &&
		    ((p->header.flag & BT_ROOT) || p->header.prev == 0))
			dtTruncateEntry(p, 0, &dtlck);

		/* unpin the parent page */
		DT_PUTPAGE(mp);

		/* exit propagation up */
		break;
	}

	return 0;
}


/*
 * NAME:        dtRelocate()
 *
 * FUNCTION:    relocate dtpage (internal or leaf) of directory;
 *              This function is mainly used by defragfs utility.
 */
int dtRelocate(tid_t tid, struct inode *ip, s64 lmxaddr, pxd_t * opxd,
	       s64 nxaddr)
{
	int rc = 0;
	metapage_t *mp, *pmp, *lmp, *rmp;
	dtpage_t *p, *pp, *rp = 0, *lp= 0;
	s64 bn;
	int index;
	btstack_t btstack;
	pxd_t *pxd;
	s64 oxaddr, nextbn, prevbn;
	int xlen, xsize;
	tlock_t *tlck;
	dtlock_t *dtlck;
	pxdlock_t *pxdlock;
	s8 *stbl;
	lv_t *lv;

	oxaddr = addressPXD(opxd);
	xlen = lengthPXD(opxd);

	jEVENT(0, ("dtRelocate: lmxaddr:%Ld xaddr:%Ld:%Ld xlen:%d\n",
		   lmxaddr, oxaddr, nxaddr, xlen));

	/*
	 *      1. get the internal parent dtpage covering
	 *      router entry for the tartget page to be relocated;
	 */
	rc = dtSearchNode(ip, lmxaddr, opxd, &btstack);
	if (rc)
		return rc;

	/* retrieve search result */
	DT_GETSEARCH(ip, btstack.top, bn, pmp, pp, index);
	jEVENT(0, ("dtRelocate: parent router entry validated.\n"));

	/*
	 *      2. relocate the target dtpage
	 */
	/* read in the target page from src extent */
	DT_GETPAGE(ip, oxaddr, mp, PSIZE, p, rc);
	if (rc) {
		/* release the pinned parent page */
		DT_PUTPAGE(pmp);
		return rc;
	}

	/*
	 * read in sibling pages if any to update sibling pointers;
	 */
	rmp = NULL;
	if (p->header.next) {
		nextbn = le64_to_cpu(p->header.next);
		DT_GETPAGE(ip, nextbn, rmp, PSIZE, rp, rc);
		if (rc) {
			DT_PUTPAGE(mp);
			DT_PUTPAGE(pmp);
			return (rc);
		}
	}

	lmp = NULL;
	if (p->header.prev) {
		prevbn = le64_to_cpu(p->header.prev);
		DT_GETPAGE(ip, prevbn, lmp, PSIZE, lp, rc);
		if (rc) {
			DT_PUTPAGE(mp);
			DT_PUTPAGE(pmp);
			if (rmp)
				DT_PUTPAGE(rmp);
			return (rc);
		}
	}

	/* at this point, all xtpages to be updated are in memory */

	/*
	 * update sibling pointers of sibling dtpages if any;
	 */
	if (lmp) {
		tlck = txLock(tid, ip, lmp, tlckDTREE | tlckRELINK);
		dtlck = (dtlock_t *) & tlck->lock;
		/* linelock header */
		ASSERT(dtlck->index == 0);
		lv = (lv_t *) & dtlck->lv[0];
		lv->offset = 0;
		lv->length = 1;
		dtlck->index++;

		lp->header.next = cpu_to_le64(nxaddr);
		DT_PUTPAGE(lmp);
	}

	if (rmp) {
		tlck = txLock(tid, ip, rmp, tlckDTREE | tlckRELINK);
		dtlck = (dtlock_t *) & tlck->lock;
		/* linelock header */
		ASSERT(dtlck->index == 0);
		lv = (lv_t *) & dtlck->lv[0];
		lv->offset = 0;
		lv->length = 1;
		dtlck->index++;

		rp->header.prev = cpu_to_le64(nxaddr);
		DT_PUTPAGE(rmp);
	}

	/*
	 * update the target dtpage to be relocated
	 *
	 * write LOG_REDOPAGE of LOG_NEW type for dst page
	 * for the whole target page (logredo() will apply
	 * after image and update bmap for allocation of the
	 * dst extent), and update bmap for allocation of
	 * the dst extent;
	 */
	tlck = txLock(tid, ip, mp, tlckDTREE | tlckNEW);
	dtlck = (dtlock_t *) & tlck->lock;
	/* linelock header */
	ASSERT(dtlck->index == 0);
	lv = (lv_t *) & dtlck->lv[0];

	/* update the self address in the dtpage header */
	pxd = &p->header.self;
	PXDaddress(pxd, nxaddr);

	/* the dst page is the same as the src page, i.e.,
	 * linelock for afterimage of the whole page;
	 */
	lv->offset = 0;
	lv->length = p->header.maxslot;
	dtlck->index++;

	/* update the buffer extent descriptor of the dtpage */
	xsize = xlen << JFS_SBI(ip->i_sb)->l2bsize;
#ifdef _STILL_TO_PORT
	bmSetXD(mp, nxaddr, xsize);
#endif /* _STILL_TO_PORT */
	/* unpin the relocated page */
	DT_PUTPAGE(mp);
	jEVENT(0, ("dtRelocate: target dtpage relocated.\n"));

	/* the moved extent is dtpage, then a LOG_NOREDOPAGE log rec
	 * needs to be written (in logredo(), the LOG_NOREDOPAGE log rec
	 * will also force a bmap update ).
	 */

	/*
	 *      3. acquire maplock for the source extent to be freed;
	 */
	/* for dtpage relocation, write a LOG_NOREDOPAGE record
	 * for the source dtpage (logredo() will init NoRedoPage
	 * filter and will also update bmap for free of the source
	 * dtpage), and upadte bmap for free of the source dtpage;
	 */
	tlck = txMaplock(tid, ip, tlckDTREE | tlckFREE);
	pxdlock = (pxdlock_t *) & tlck->lock;
	pxdlock->flag = mlckFREEPXD;
	PXDaddress(&pxdlock->pxd, oxaddr);
	PXDlength(&pxdlock->pxd, xlen);
	pxdlock->index = 1;

	/*
	 *      4. update the parent router entry for relocation;
	 *
	 * acquire tlck for the parent entry covering the target dtpage;
	 * write LOG_REDOPAGE to apply after image only;
	 */
	jEVENT(0, ("dtRelocate: update parent router entry.\n"));
	tlck = txLock(tid, ip, pmp, tlckDTREE | tlckENTRY);
	dtlck = (dtlock_t *) & tlck->lock;
	lv = (lv_t *) & dtlck->lv[dtlck->index];

	/* update the PXD with the new address */
	stbl = DT_GETSTBL(pp);
	pxd = (pxd_t *) & pp->slot[stbl[index]];
	PXDaddress(pxd, nxaddr);
	lv->offset = stbl[index];
	lv->length = 1;
	dtlck->index++;

	/* unpin the parent dtpage */
	DT_PUTPAGE(pmp);

	return rc;
}


/*
 * NAME:	dtSearchNode()
 *
 * FUNCTION:	Search for an dtpage containing a specified address
 *              This function is mainly used by defragfs utility.
 *
 * NOTE:	Search result on stack, the found page is pinned at exit.
 *		The result page must be an internal dtpage.
 *		lmxaddr give the address of the left most page of the
 *		dtree level, in which the required dtpage resides.
 */
static int dtSearchNode(struct inode *ip, s64 lmxaddr, pxd_t * kpxd,
			btstack_t * btstack)
{
	int rc = 0;
	s64 bn;
	metapage_t *mp;
	dtpage_t *p;
	int psize = 288;	/* initial in-line directory */
	s8 *stbl;
	int i;
	pxd_t *pxd;
	btframe_t *btsp;

	BT_CLR(btstack);	/* reset stack */

	/*
	 *      descend tree to the level with specified leftmost page
	 *
	 *  by convention, root bn = 0.
	 */
	for (bn = 0;;) {
		/* get/pin the page to search */
		DT_GETPAGE(ip, bn, mp, psize, p, rc);
		if (rc)
			return rc;

		/* does the xaddr of leftmost page of the levevl
		 * matches levevl search key ?
		 */
		if (p->header.flag & BT_ROOT) {
			if (lmxaddr == 0)
				break;
		} else if (addressPXD(&p->header.self) == lmxaddr)
			break;

		/*
		 * descend down to leftmost child page
		 */
		if (p->header.flag & BT_LEAF)
			return ESTALE;

		/* get the leftmost entry */
		stbl = DT_GETSTBL(p);
		pxd = (pxd_t *) & p->slot[stbl[0]];

		/* get the child page block address */
		bn = addressPXD(pxd);
		psize = lengthPXD(pxd) << JFS_SBI(ip->i_sb)->l2bsize;
		/* unpin the parent page */
		DT_PUTPAGE(mp);
	}

	/*
	 *      search each page at the current levevl
	 */
      loop:
	stbl = DT_GETSTBL(p);
	for (i = 0; i < p->header.nextindex; i++) {
		pxd = (pxd_t *) & p->slot[stbl[i]];

		/* found the specified router entry */
		if (addressPXD(pxd) == addressPXD(kpxd) &&
		    lengthPXD(pxd) == lengthPXD(kpxd)) {
			btsp = btstack->top;
			btsp->bn = bn;
			btsp->index = i;
			btsp->mp = mp;

			return 0;
		}
	}

	/* get the right sibling page if any */
	if (p->header.next)
		bn = le64_to_cpu(p->header.next);
	else {
		DT_PUTPAGE(mp);
		return ESTALE;
	}

	/* unpin current page */
	DT_PUTPAGE(mp);

	/* get the right sibling page */
	DT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
	if (rc)
		return rc;

	goto loop;
}


/*
 *	dtRelink()
 *
 * function:
 *	link around a freed page.
 *
 * parameter:
 *	fp:	page to be freed
 *
 * return:
 */
static int dtRelink(tid_t tid, struct inode *ip, dtpage_t * p)
{
	int rc;
	metapage_t *mp;
	s64 nextbn, prevbn;
	tlock_t *tlck;
	dtlock_t *dtlck;
	lv_t *lv;

	nextbn = le64_to_cpu(p->header.next);
	prevbn = le64_to_cpu(p->header.prev);

	/* update prev pointer of the next page */
	if (nextbn != 0) {
		DT_GETPAGE(ip, nextbn, mp, PSIZE, p, rc);
		if (rc)
			return rc;

		BT_MARK_DIRTY(mp, ip);
		/*
		 * acquire a transaction lock on the next page
		 *
		 * action: update prev pointer;
		 */
		tlck = txLock(tid, ip, mp, tlckDTREE | tlckRELINK);
		jEVENT(0,
		       ("dtRelink nextbn: tlck = 0x%p, ip = 0x%p, mp=0x%p\n",
			tlck, ip, mp));
		dtlck = (dtlock_t *) & tlck->lock;

		/* linelock header */
		if (dtlck->index >= dtlck->maxcnt)
			dtlck = (dtlock_t *) txLinelock(dtlck);
		lv = (lv_t *) & dtlck->lv[dtlck->index];
		lv->offset = 0;
		lv->length = 1;
		dtlck->index++;

		p->header.prev = cpu_to_le64(prevbn);
		DT_PUTPAGE(mp);
	}

	/* update next pointer of the previous page */
	if (prevbn != 0) {
		DT_GETPAGE(ip, prevbn, mp, PSIZE, p, rc);
		if (rc)
			return rc;

		BT_MARK_DIRTY(mp, ip);
		/*
		 * acquire a transaction lock on the prev page
		 *
		 * action: update next pointer;
		 */
		tlck = txLock(tid, ip, mp, tlckDTREE | tlckRELINK);
		jEVENT(0,
		       ("dtRelink prevbn: tlck = 0x%p, ip = 0x%p, mp=0x%p\n",
			tlck, ip, mp));
		dtlck = (dtlock_t *) & tlck->lock;

		/* linelock header */
		if (dtlck->index >= dtlck->maxcnt)
			dtlck = (dtlock_t *) txLinelock(dtlck);
		lv = (lv_t *) & dtlck->lv[dtlck->index];
		lv->offset = 0;
		lv->length = 1;
		dtlck->index++;

		p->header.next = cpu_to_le64(nextbn);
		DT_PUTPAGE(mp);
	}

	return 0;
}


/*
 *	dtInitRoot()
 *
 * initialize directory root (inline in inode)
 */
void dtInitRoot(tid_t tid, struct inode *ip, u32 idotdot)
{
	struct jfs_inode_info *jfs_ip = JFS_IP(ip);
	dtroot_t *p;
	int fsi;
	dtslot_t *f;
	tlock_t *tlck;
	dtlock_t *dtlck;
	lv_t *lv;
	u16 xflag_save;

	/*
	 * If this was previously an non-empty directory, we need to remove
	 * the old directory table.
	 */
	if (DO_INDEX(ip)) {
		if (jfs_ip->next_index > (MAX_INLINE_DIRTABLE_ENTRY + 1)) {
			tblock_t *tblk = tid_to_tblock(tid);
			/*
			 * We're playing games with the tid's xflag.  If
			 * we're removing a regular file, the file's xtree
			 * is committed with COMMIT_PMAP, but we always
			 * commit the directories xtree with COMMIT_PWMAP.
			 */
			xflag_save = tblk->xflag;
			tblk->xflag = 0;
			/*
			 * xtTruncate isn't guaranteed to fully truncate
			 * the xtree.  The caller needs to check i_size
			 * after committing the transaction to see if
			 * additional truncation is needed.  The
			 * COMMIT_Stale flag tells caller that we
			 * initiated the truncation.
			 */
			xtTruncate(tid, ip, 0, COMMIT_PWMAP);
			set_cflag(COMMIT_Stale, ip);

			tblk->xflag = xflag_save;
			/*
			 * Tells jfs_metapage code that the metadata pages
			 * for the index table are no longer useful, and
			 * remove them from page cache.
			 */
			invalidate_inode_metapages(ip);
		} else
			ip->i_size = 1;

		jfs_ip->next_index = 2;
	} else
		ip->i_size = IDATASIZE;

	/*
	 * acquire a transaction lock on the root
	 *
	 * action: directory initialization;
	 */
	tlck = txLock(tid, ip, (metapage_t *) & jfs_ip->bxflag,
		      tlckDTREE | tlckENTRY | tlckBTROOT);
	dtlck = (dtlock_t *) & tlck->lock;

	/* linelock root */
	ASSERT(dtlck->index == 0);
	lv = (lv_t *) & dtlck->lv[0];
	lv->offset = 0;
	lv->length = DTROOTMAXSLOT;
	dtlck->index++;

	p = &jfs_ip->i_dtroot;

	p->header.flag = DXD_INDEX | BT_ROOT | BT_LEAF;

	p->header.nextindex = 0;

	/* init freelist */
	fsi = 1;
	f = &p->slot[fsi];

	/* init data area of root */
	for (fsi++; fsi < DTROOTMAXSLOT; f++, fsi++)
		f->next = fsi;
	f->next = -1;

	p->header.freelist = 1;
	p->header.freecnt = 8;

	/* init '..' entry */
	p->header.idotdot = cpu_to_le32(idotdot);

#if 0
	ip->i_blocks = LBLK2PBLK(ip->i_sb,
				 ((jfs_ip->ea.flag & DXD_EXTENT) ?
				  lengthDXD(&jfs_ip->ea) : 0) +
				 ((jfs_ip->acl.flag & DXD_EXTENT) ?
				  lengthDXD(&jfs_ip->acl) : 0));
#endif

	return;
}

/*
 *	jfs_readdir()
 *
 * function: read directory entries sequentially
 *	from the specified entry offset
 *
 * parameter:
 *
 * return: offset = (pn, index) of start entry
 *	of next jfs_readdir()/dtRead()
 */
int jfs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct inode *ip = filp->f_dentry->d_inode;
	struct nls_table *codepage = JFS_SBI(ip->i_sb)->nls_tab;
	int rc = 0;
	struct dtoffset {
		s16 pn;
		s16 index;
		s32 unused;
	} *dtoffset = (struct dtoffset *) &filp->f_pos;
	s64 bn;
	metapage_t *mp;
	dtpage_t *p;
	int index;
	s8 *stbl;
	btstack_t btstack;
	int i, next;
	ldtentry_t *d;
	dtslot_t *t;
	int d_namleft, d_namlen, len, outlen;
	char *d_name, *name_ptr;
	int dtlhdrdatalen;
	u32 dir_index;
	int do_index = 0;
	uint loop_count = 0;

	if (filp->f_pos == DIREND)
		return 0;

	lock_kernel();

	if (DO_INDEX(ip)) {
		/*
		 * persistent index is stored in directory entries.
		 * Special cases:        0 = .
		 *                       1 = ..
		 *                      -1 = End of directory
		 */
		do_index = 1;
		dtlhdrdatalen = DTLHDRDATALEN;

		dir_index = (u32) filp->f_pos;

		if (dir_index > 1) {
			dir_table_slot_t dirtab_slot;

			if (dtEmpty(ip)) {
				filp->f_pos = DIREND;
				unlock_kernel();
				return 0;
			}
		      repeat:
			rc = get_index(ip, dir_index, &dirtab_slot);
			if (rc) {
				filp->f_pos = DIREND;
				unlock_kernel();
				return rc;
			}
			if (dirtab_slot.flag == DIR_INDEX_FREE) {
				if (loop_count++ > JFS_IP(ip)->next_index) {
					jERROR(1, ("jfs_readdir detected "
						   "infinite loop!\n"));
					filp->f_pos = DIREND;
					unlock_kernel();
					return 0;
				}
				dir_index = le32_to_cpu(dirtab_slot.addr2);
				if (dir_index == -1) {
					filp->f_pos = DIREND;
					unlock_kernel();
					return 0;
				}
				goto repeat;
			}
			bn = addressDTS(&dirtab_slot);
			index = dirtab_slot.slot;
			DT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
			if (rc) {
				filp->f_pos = DIREND;
				unlock_kernel();
				return 0;
			}
			if (p->header.flag & BT_INTERNAL) {
				jERROR(1,("jfs_readdir: bad index table\n"));
				DT_PUTPAGE(mp);
				filp->f_pos = -1;
				unlock_kernel();
				return 0;
			}
		} else {
			if (dir_index == 0) {
				/*
				 * self "."
				 */
				filp->f_pos = 0;
				if (filldir(dirent, ".", 1, 0, ip->i_ino,
					    DT_DIR)) {
					unlock_kernel();	
					return 0;
				}
			}
			/*
			 * parent ".."
			 */
			filp->f_pos = 1;
			if (filldir
			    (dirent, "..", 2, 1, PARENT(ip), DT_DIR)) {
				unlock_kernel();
				return 0;
			}

			/*
			 * Find first entry of left-most leaf
			 */
			if (dtEmpty(ip)) {
				filp->f_pos = DIREND;
				unlock_kernel();
				return 0;
			}

			if ((rc = dtReadFirst(ip, &btstack))) {
				unlock_kernel();
				return -rc;
			}

			DT_GETSEARCH(ip, btstack.top, bn, mp, p, index);
		}
	} else {
		/*
		 * Legacy filesystem - OS/2 & Linux JFS < 0.3.6
		 *
		 * pn = index = 0:      First entry "."
		 * pn = 0; index = 1:   Second entry ".."
		 * pn > 0:              Real entries, pn=1 -> leftmost page
		 * pn = index = -1:     No more entries
		 */
		dtlhdrdatalen = DTLHDRDATALEN_LEGACY;

		if (filp->f_pos == 0) {
			/* build "." entry */

			if (filldir(dirent, ".", 1, filp->f_pos, ip->i_ino,
				    DT_DIR)) {
				unlock_kernel();
				return 0;
			}
			dtoffset->index = 1;
		}

		if (dtoffset->pn == 0) {
			if (dtoffset->index == 1) {
				/* build ".." entry */

				if (filldir(dirent, "..", 2, filp->f_pos,
					    PARENT(ip), DT_DIR)) {
					unlock_kernel();
					return 0;
				}
			} else {
				jERROR(1,
				       ("jfs_readdir called with invalid offset!\n"));
			}
			dtoffset->pn = 1;
			dtoffset->index = 0;
		}

		if (dtEmpty(ip)) {
			filp->f_pos = DIREND;
			unlock_kernel();
			return 0;
		}

		if ((rc = dtReadNext(ip, &filp->f_pos, &btstack))) {
			jERROR(1,
			       ("jfs_readdir: unexpected rc = %d from dtReadNext\n",
				rc));
			filp->f_pos = DIREND;
			unlock_kernel();
			return 0;
		}
		/* get start leaf page and index */
		DT_GETSEARCH(ip, btstack.top, bn, mp, p, index);

		/* offset beyond directory eof ? */
		if (bn < 0) {
			filp->f_pos = DIREND;
			unlock_kernel();
			return 0;
		}
	}

	d_name = kmalloc((JFS_NAME_MAX + 1) * sizeof(wchar_t), GFP_NOFS);
	if (d_name == NULL) {
		DT_PUTPAGE(mp);
		jERROR(1, ("jfs_readdir: kmalloc failed!\n"));
		filp->f_pos = DIREND;
		unlock_kernel();
		return 0;
	}
	while (1) {
		stbl = DT_GETSTBL(p);

		for (i = index; i < p->header.nextindex; i++) {
			d = (ldtentry_t *) & p->slot[stbl[i]];

			d_namleft = d->namlen;
			name_ptr = d_name;

			if (do_index) {
				filp->f_pos = le32_to_cpu(d->index);
				len = min(d_namleft, DTLHDRDATALEN);
			} else
				len = min(d_namleft, DTLHDRDATALEN_LEGACY);

			/* copy the name of head/only segment */
			outlen = jfs_strfromUCS_le(name_ptr, d->name, len,
						   codepage);
			d_namlen = outlen;

			/* copy name in the additional segment(s) */
			next = d->next;
			while (next >= 0) {
				t = (dtslot_t *) & p->slot[next];
				name_ptr += outlen;
				d_namleft -= len;
				/* Sanity Check */
				if (d_namleft == 0) {
					jERROR(1,("JFS:Dtree error: "
					  "ino = %ld, bn=%Ld, index = %d\n",
						  ip->i_ino, bn, i));
					updateSuper(ip->i_sb, FM_DIRTY);
					goto skip_one;
				}
				len = min(d_namleft, DTSLOTDATALEN);
				outlen = jfs_strfromUCS_le(name_ptr, t->name,
							   len, codepage);
				d_namlen+= outlen;

				next = t->next;
			}

			if (filldir(dirent, d_name, d_namlen, filp->f_pos,
				    le32_to_cpu(d->inumber), DT_UNKNOWN))
				goto out;
skip_one:
			if (!do_index)
				dtoffset->index++;
		}

		/*
		 * get next leaf page
		 */

		if (p->header.flag & BT_ROOT) {
			filp->f_pos = DIREND;
			break;
		}

		bn = le64_to_cpu(p->header.next);
		if (bn == 0) {
			filp->f_pos = DIREND;
			break;
		}

		/* unpin previous leaf page */
		DT_PUTPAGE(mp);

		/* get next leaf page */
		DT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
		if (rc) {
			kfree(d_name);
			unlock_kernel();
			return -rc;
		}

		/* update offset (pn:index) for new page */
		index = 0;
		if (!do_index) {
			dtoffset->pn++;
			dtoffset->index = 0;
		}

	}

      out:
	kfree(d_name);
	DT_PUTPAGE(mp);
	unlock_kernel();
	return rc;
}


/*
 *	dtReadFirst()
 *
 * function: get the leftmost page of the directory
 */
static int dtReadFirst(struct inode *ip, btstack_t * btstack)
{
	int rc = 0;
	s64 bn;
	int psize = 288;	/* initial in-line directory */
	metapage_t *mp;
	dtpage_t *p;
	s8 *stbl;
	btframe_t *btsp;
	pxd_t *xd;

	BT_CLR(btstack);	/* reset stack */

	/*
	 *      descend leftmost path of the tree
	 *
	 * by convention, root bn = 0.
	 */
	for (bn = 0;;) {
		DT_GETPAGE(ip, bn, mp, psize, p, rc);
		if (rc)
			return rc;

		/*
		 * leftmost leaf page
		 */
		if (p->header.flag & BT_LEAF) {
			/* return leftmost entry */
			btsp = btstack->top;
			btsp->bn = bn;
			btsp->index = 0;
			btsp->mp = mp;

			return 0;
		}

		/*
		 * descend down to leftmost child page
		 */
		/* push (bn, index) of the parent page/entry */
		BT_PUSH(btstack, bn, 0);

		/* get the leftmost entry */
		stbl = DT_GETSTBL(p);
		xd = (pxd_t *) & p->slot[stbl[0]];

		/* get the child page block address */
		bn = addressPXD(xd);
		psize = lengthPXD(xd) << JFS_SBI(ip->i_sb)->l2bsize;

		/* unpin the parent page */
		DT_PUTPAGE(mp);
	}
}


/*
 *	dtReadNext()
 *
 * function: get the page of the specified offset (pn:index)
 *
 * return: if (offset > eof), bn = -1;
 *
 * note: if index > nextindex of the target leaf page,
 * start with 1st entry of next leaf page;
 */
static int dtReadNext(struct inode *ip, loff_t * offset, btstack_t * btstack)
{
	int rc = 0;
	struct dtoffset {
		s16 pn;
		s16 index;
		s32 unused;
	} *dtoffset = (struct dtoffset *) offset;
	s64 bn;
	metapage_t *mp;
	dtpage_t *p;
	int index;
	int pn;
	s8 *stbl;
	btframe_t *btsp, *parent;
	pxd_t *xd;

	/*
	 * get leftmost leaf page pinned
	 */
	if ((rc = dtReadFirst(ip, btstack)))
		return rc;

	/* get leaf page */
	DT_GETSEARCH(ip, btstack->top, bn, mp, p, index);

	/* get the start offset (pn:index) */
	pn = dtoffset->pn - 1;	/* Now pn = 0 represents leftmost leaf */
	index = dtoffset->index;

	/* start at leftmost page ? */
	if (pn == 0) {
		/* offset beyond eof ? */
		if (index < p->header.nextindex)
			goto out;

		if (p->header.flag & BT_ROOT) {
			bn = -1;
			goto out;
		}

		/* start with 1st entry of next leaf page */
		dtoffset->pn++;
		dtoffset->index = index = 0;
		goto a;
	}

	/* start at non-leftmost page: scan parent pages for large pn */
	if (p->header.flag & BT_ROOT) {
		bn = -1;
		goto out;
	}

	/* start after next leaf page ? */
	if (pn > 1)
		goto b;

	/* get leaf page pn = 1 */
      a:
	bn = le64_to_cpu(p->header.next);

	/* unpin leaf page */
	DT_PUTPAGE(mp);

	/* offset beyond eof ? */
	if (bn == 0) {
		bn = -1;
		goto out;
	}

	goto c;

	/*
	 * scan last internal page level to get target leaf page
	 */
      b:
	/* unpin leftmost leaf page */
	DT_PUTPAGE(mp);

	/* get left most parent page */
	btsp = btstack->top;
	parent = btsp - 1;
	bn = parent->bn;
	DT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
	if (rc)
		return rc;

	/* scan parent pages at last internal page level */
	while (pn >= p->header.nextindex) {
		pn -= p->header.nextindex;

		/* get next parent page address */
		bn = le64_to_cpu(p->header.next);

		/* unpin current parent page */
		DT_PUTPAGE(mp);

		/* offset beyond eof ? */
		if (bn == 0) {
			bn = -1;
			goto out;
		}

		/* get next parent page */
		DT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
		if (rc)
			return rc;

		/* update parent page stack frame */
		parent->bn = bn;
	}

	/* get leaf page address */
	stbl = DT_GETSTBL(p);
	xd = (pxd_t *) & p->slot[stbl[pn]];
	bn = addressPXD(xd);

	/* unpin parent page */
	DT_PUTPAGE(mp);

	/*
	 * get target leaf page
	 */
      c:
	DT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
	if (rc)
		return rc;

	/*
	 * leaf page has been completed:
	 * start with 1st entry of next leaf page
	 */
	if (index >= p->header.nextindex) {
		bn = le64_to_cpu(p->header.next);

		/* unpin leaf page */
		DT_PUTPAGE(mp);

		/* offset beyond eof ? */
		if (bn == 0) {
			bn = -1;
			goto out;
		}

		/* get next leaf page */
		DT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
		if (rc)
			return rc;

		/* start with 1st entry of next leaf page */
		dtoffset->pn++;
		dtoffset->index = 0;
	}

      out:
	/* return target leaf page pinned */
	btsp = btstack->top;
	btsp->bn = bn;
	btsp->index = dtoffset->index;
	btsp->mp = mp;

	return 0;
}


/*
 *	dtCompare()
 *
 * function: compare search key with an internal entry
 *
 * return:
 *	< 0 if k is < record
 *	= 0 if k is = record
 *	> 0 if k is > record
 */
static int dtCompare(component_t * key,	/* search key */
		     dtpage_t * p,	/* directory page */
		     int si)
{				/* entry slot index */
	wchar_t *kname, *name;
	int klen, namlen, len, rc;
	idtentry_t *ih;
	dtslot_t *t;

	/*
	 * force the left-most key on internal pages, at any level of
	 * the tree, to be less than any search key.
	 * this obviates having to update the leftmost key on an internal
	 * page when the user inserts a new key in the tree smaller than
	 * anything that has been stored.
	 *
	 * (? if/when dtSearch() narrows down to 1st entry (index = 0),
	 * at any internal page at any level of the tree,
	 * it descends to child of the entry anyway -
	 * ? make the entry as min size dummy entry)
	 *
	 * if (e->index == 0 && h->prevpg == P_INVALID && !(h->flags & BT_LEAF))
	 * return (1);
	 */

	kname = key->name;
	klen = key->namlen;

	ih = (idtentry_t *) & p->slot[si];
	si = ih->next;
	name = ih->name;
	namlen = ih->namlen;
	len = min(namlen, DTIHDRDATALEN);

	/* compare with head/only segment */
	len = min(klen, len);
	if ((rc = UniStrncmp_le(kname, name, len)))
		return rc;

	klen -= len;
	namlen -= len;

	/* compare with additional segment(s) */
	kname += len;
	while (klen > 0 && namlen > 0) {
		/* compare with next name segment */
		t = (dtslot_t *) & p->slot[si];
		len = min(namlen, DTSLOTDATALEN);
		len = min(klen, len);
		name = t->name;
		if ((rc = UniStrncmp_le(kname, name, len)))
			return rc;

		klen -= len;
		namlen -= len;
		kname += len;
		si = t->next;
	}

	return (klen - namlen);
}




/*
 *	ciCompare()
 *
 * function: compare search key with an (leaf/internal) entry
 *
 * return:
 *	< 0 if k is < record
 *	= 0 if k is = record
 *	> 0 if k is > record
 */
static int ciCompare(component_t * key,	/* search key */
		     dtpage_t * p,	/* directory page */
		     int si,	/* entry slot index */
		     int flag)
{
	wchar_t *kname, *name, x;
	int klen, namlen, len, rc;
	ldtentry_t *lh;
	idtentry_t *ih;
	dtslot_t *t;
	int i;

	/*
	 * force the left-most key on internal pages, at any level of
	 * the tree, to be less than any search key.
	 * this obviates having to update the leftmost key on an internal
	 * page when the user inserts a new key in the tree smaller than
	 * anything that has been stored.
	 *
	 * (? if/when dtSearch() narrows down to 1st entry (index = 0),
	 * at any internal page at any level of the tree,
	 * it descends to child of the entry anyway -
	 * ? make the entry as min size dummy entry)
	 *
	 * if (e->index == 0 && h->prevpg == P_INVALID && !(h->flags & BT_LEAF))
	 * return (1);
	 */

	kname = key->name;
	klen = key->namlen;

	/*
	 * leaf page entry
	 */
	if (p->header.flag & BT_LEAF) {
		lh = (ldtentry_t *) & p->slot[si];
		si = lh->next;
		name = lh->name;
		namlen = lh->namlen;
		if (flag & JFS_DIR_INDEX)
			len = min(namlen, DTLHDRDATALEN);
		else
			len = min(namlen, DTLHDRDATALEN_LEGACY);
	}
	/*
	 * internal page entry
	 */
	else {
		ih = (idtentry_t *) & p->slot[si];
		si = ih->next;
		name = ih->name;
		namlen = ih->namlen;
		len = min(namlen, DTIHDRDATALEN);
	}

	/* compare with head/only segment */
	len = min(klen, len);
	for (i = 0; i < len; i++, kname++, name++) {
		/* only uppercase if case-insensitive support is on */
		if ((flag & JFS_OS2) == JFS_OS2)
			x = UniToupper(le16_to_cpu(*name));
		else
			x = le16_to_cpu(*name);
		if ((rc = *kname - x))
			return rc;
	}

	klen -= len;
	namlen -= len;

	/* compare with additional segment(s) */
	while (klen > 0 && namlen > 0) {
		/* compare with next name segment */
		t = (dtslot_t *) & p->slot[si];
		len = min(namlen, DTSLOTDATALEN);
		len = min(klen, len);
		name = t->name;
		for (i = 0; i < len; i++, kname++, name++) {
			/* only uppercase if case-insensitive support is on */
			if ((flag & JFS_OS2) == JFS_OS2)
				x = UniToupper(le16_to_cpu(*name));
			else
				x = le16_to_cpu(*name);

			if ((rc = *kname - x))
				return rc;
		}

		klen -= len;
		namlen -= len;
		si = t->next;
	}

	return (klen - namlen);
}


/*
 *	ciGetLeafPrefixKey()
 *
 * function: compute prefix of suffix compression
 *	     from two adjacent leaf entries
 *	     across page boundary
 *
 * return:
 *	Number of prefix bytes needed to distinguish b from a.
 */
static void ciGetLeafPrefixKey(dtpage_t * lp, int li, dtpage_t * rp,
			       int ri, component_t * key, int flag)
{
	int klen, namlen;
	wchar_t *pl, *pr, *kname;
	wchar_t lname[JFS_NAME_MAX + 1];
	component_t lkey = { 0, lname };
	wchar_t rname[JFS_NAME_MAX + 1];
	component_t rkey = { 0, rname };

	/* get left and right key */
	dtGetKey(lp, li, &lkey, flag);
	lkey.name[lkey.namlen] = 0;

	if ((flag & JFS_OS2) == JFS_OS2)
		ciToUpper(&lkey);

	dtGetKey(rp, ri, &rkey, flag);
	rkey.name[rkey.namlen] = 0;


	if ((flag & JFS_OS2) == JFS_OS2)
		ciToUpper(&rkey);

	/* compute prefix */
	klen = 0;
	kname = key->name;
	namlen = min(lkey.namlen, rkey.namlen);
	for (pl = lkey.name, pr = rkey.name;
	     namlen; pl++, pr++, namlen--, klen++, kname++) {
		*kname = *pr;
		if (*pl != *pr) {
			key->namlen = klen + 1;
			return;
		}
	}

	/* l->namlen <= r->namlen since l <= r */
	if (lkey.namlen < rkey.namlen) {
		*kname = *pr;
		key->namlen = klen + 1;
	} else			/* l->namelen == r->namelen */
		key->namlen = klen;

	return;
}



/*
 *	dtGetKey()
 *
 * function: get key of the entry
 */
static void dtGetKey(dtpage_t * p, int i,	/* entry index */
		     component_t * key, int flag)
{
	int si;
	s8 *stbl;
	ldtentry_t *lh;
	idtentry_t *ih;
	dtslot_t *t;
	int namlen, len;
	wchar_t *name, *kname;

	/* get entry */
	stbl = DT_GETSTBL(p);
	si = stbl[i];
	if (p->header.flag & BT_LEAF) {
		lh = (ldtentry_t *) & p->slot[si];
		si = lh->next;
		namlen = lh->namlen;
		name = lh->name;
		if (flag & JFS_DIR_INDEX)
			len = min(namlen, DTLHDRDATALEN);
		else
			len = min(namlen, DTLHDRDATALEN_LEGACY);
	} else {
		ih = (idtentry_t *) & p->slot[si];
		si = ih->next;
		namlen = ih->namlen;
		name = ih->name;
		len = min(namlen, DTIHDRDATALEN);
	}

	key->namlen = namlen;
	kname = key->name;

	/*
	 * move head/only segment
	 */
	UniStrncpy_le(kname, name, len);

	/*
	 * move additional segment(s)
	 */
	while (si >= 0) {
		/* get next segment */
		t = &p->slot[si];
		kname += len;
		namlen -= len;
		len = min(namlen, DTSLOTDATALEN);
		UniStrncpy_le(kname, t->name, len);

		si = t->next;
	}
}


/*
 *	dtInsertEntry()
 *
 * function: allocate free slot(s) and
 *	     write a leaf/internal entry
 *
 * return: entry slot index
 */
static void dtInsertEntry(dtpage_t * p, int index, component_t * key,
			  ddata_t * data, dtlock_t ** dtlock)
{
	dtslot_t *h, *t;
	ldtentry_t *lh = 0;
	idtentry_t *ih = 0;
	int hsi, fsi, klen, len, nextindex;
	wchar_t *kname, *name;
	s8 *stbl;
	pxd_t *xd;
	dtlock_t *dtlck = *dtlock;
	lv_t *lv;
	int xsi, n;
	s64 bn = 0;
	metapage_t *mp = 0;

	klen = key->namlen;
	kname = key->name;

	/* allocate a free slot */
	hsi = fsi = p->header.freelist;
	h = &p->slot[fsi];
	p->header.freelist = h->next;
	--p->header.freecnt;

	/* open new linelock */
	if (dtlck->index >= dtlck->maxcnt)
		dtlck = (dtlock_t *) txLinelock(dtlck);

	lv = (lv_t *) & dtlck->lv[dtlck->index];
	lv->offset = hsi;

	/* write head/only segment */
	if (p->header.flag & BT_LEAF) {
		lh = (ldtentry_t *) h;
		lh->next = h->next;
		lh->inumber = data->leaf.ino;	/* little-endian */
		lh->namlen = klen;
		name = lh->name;
		if (data->leaf.ip) {
			len = min(klen, DTLHDRDATALEN);
			if (!(p->header.flag & BT_ROOT))
				bn = addressPXD(&p->header.self);
			lh->index = cpu_to_le32(add_index(data->leaf.tid,
							  data->leaf.ip,
							  bn, index));
		} else
			len = min(klen, DTLHDRDATALEN_LEGACY);
	} else {
		ih = (idtentry_t *) h;
		ih->next = h->next;
		xd = (pxd_t *) ih;
		*xd = data->xd;
		ih->namlen = klen;
		name = ih->name;
		len = min(klen, DTIHDRDATALEN);
	}

	UniStrncpy_le(name, kname, len);

	n = 1;
	xsi = hsi;

	/* write additional segment(s) */
	t = h;
	klen -= len;
	while (klen) {
		/* get free slot */
		fsi = p->header.freelist;
		t = &p->slot[fsi];
		p->header.freelist = t->next;
		--p->header.freecnt;

		/* is next slot contiguous ? */
		if (fsi != xsi + 1) {
			/* close current linelock */
			lv->length = n;
			dtlck->index++;

			/* open new linelock */
			if (dtlck->index < dtlck->maxcnt)
				lv++;
			else {
				dtlck = (dtlock_t *) txLinelock(dtlck);
				lv = (lv_t *) & dtlck->lv[0];
			}

			lv->offset = fsi;
			n = 0;
		}

		kname += len;
		len = min(klen, DTSLOTDATALEN);
		UniStrncpy_le(t->name, kname, len);

		n++;
		xsi = fsi;
		klen -= len;
	}

	/* close current linelock */
	lv->length = n;
	dtlck->index++;

	*dtlock = dtlck;

	/* terminate last/only segment */
	if (h == t) {
		/* single segment entry */
		if (p->header.flag & BT_LEAF)
			lh->next = -1;
		else
			ih->next = -1;
	} else
		/* multi-segment entry */
		t->next = -1;

	/* if insert into middle, shift right succeeding entries in stbl */
	stbl = DT_GETSTBL(p);
	nextindex = p->header.nextindex;
	if (index < nextindex) {
		memmove(stbl + index + 1, stbl + index, nextindex - index);

		if ((p->header.flag & BT_LEAF) && data->leaf.ip) {
			/*
			 * Need to update slot number for entries that moved
			 * in the stbl
			 */
			mp = 0;
			for (n = index + 1; n <= nextindex; n++) {
				lh = (ldtentry_t *) & (p->slot[stbl[n]]);
				modify_index(data->leaf.tid, data->leaf.ip,
					     le32_to_cpu(lh->index), bn, n,
					     &mp);
			}
			if (mp)
				release_metapage(mp);
		}
	}

	stbl[index] = hsi;

	/* advance next available entry index of stbl */
	++p->header.nextindex;
}


/*
 *	dtMoveEntry()
 *
 * function: move entries from split/left page to new/right page
 *
 *	nextindex of dst page and freelist/freecnt of both pages
 *	are updated.
 */
static void dtMoveEntry(dtpage_t * sp, int si, dtpage_t * dp,
			dtlock_t ** sdtlock, dtlock_t ** ddtlock,
			int do_index)
{
	int ssi, next;		/* src slot index */
	int di;			/* dst entry index */
	int dsi;		/* dst slot index */
	s8 *sstbl, *dstbl;	/* sorted entry table */
	int snamlen, len;
	ldtentry_t *slh, *dlh = 0;
	idtentry_t *sih, *dih = 0;
	dtslot_t *h, *s, *d;
	dtlock_t *sdtlck = *sdtlock, *ddtlck = *ddtlock;
	lv_t *slv, *dlv;
	int xssi, ns, nd;
	int sfsi;

	sstbl = (s8 *) & sp->slot[sp->header.stblindex];
	dstbl = (s8 *) & dp->slot[dp->header.stblindex];

	dsi = dp->header.freelist;	/* first (whole page) free slot */
	sfsi = sp->header.freelist;

	/* linelock destination entry slot */
	dlv = (lv_t *) & ddtlck->lv[ddtlck->index];
	dlv->offset = dsi;

	/* linelock source entry slot */
	slv = (lv_t *) & sdtlck->lv[sdtlck->index];
	slv->offset = sstbl[si];
	xssi = slv->offset - 1;

	/*
	 * move entries
	 */
	ns = nd = 0;
	for (di = 0; si < sp->header.nextindex; si++, di++) {
		ssi = sstbl[si];
		dstbl[di] = dsi;

		/* is next slot contiguous ? */
		if (ssi != xssi + 1) {
			/* close current linelock */
			slv->length = ns;
			sdtlck->index++;

			/* open new linelock */
			if (sdtlck->index < sdtlck->maxcnt)
				slv++;
			else {
				sdtlck = (dtlock_t *) txLinelock(sdtlck);
				slv = (lv_t *) & sdtlck->lv[0];
			}

			slv->offset = ssi;
			ns = 0;
		}

		/*
		 * move head/only segment of an entry
		 */
		/* get dst slot */
		h = d = &dp->slot[dsi];

		/* get src slot and move */
		s = &sp->slot[ssi];
		if (sp->header.flag & BT_LEAF) {
			/* get source entry */
			slh = (ldtentry_t *) s;
			dlh = (ldtentry_t *) h;
			snamlen = slh->namlen;

			if (do_index) {
				len = min(snamlen, DTLHDRDATALEN);
				dlh->index = slh->index; /* little-endian */
			} else
				len = min(snamlen, DTLHDRDATALEN_LEGACY);

			memcpy(dlh, slh, 6 + len * 2);

			next = slh->next;

			/* update dst head/only segment next field */
			dsi++;
			dlh->next = dsi;
		} else {
			sih = (idtentry_t *) s;
			snamlen = sih->namlen;

			len = min(snamlen, DTIHDRDATALEN);
			dih = (idtentry_t *) h;
			memcpy(dih, sih, 10 + len * 2);
			next = sih->next;

			dsi++;
			dih->next = dsi;
		}

		/* free src head/only segment */
		s->next = sfsi;
		s->cnt = 1;
		sfsi = ssi;

		ns++;
		nd++;
		xssi = ssi;

		/*
		 * move additional segment(s) of the entry
		 */
		snamlen -= len;
		while ((ssi = next) >= 0) {
			/* is next slot contiguous ? */
			if (ssi != xssi + 1) {
				/* close current linelock */
				slv->length = ns;
				sdtlck->index++;

				/* open new linelock */
				if (sdtlck->index < sdtlck->maxcnt)
					slv++;
				else {
					sdtlck =
					    (dtlock_t *)
					    txLinelock(sdtlck);
					slv = (lv_t *) & sdtlck->lv[0];
				}

				slv->offset = ssi;
				ns = 0;
			}

			/* get next source segment */
			s = &sp->slot[ssi];

			/* get next destination free slot */
			d++;

			len = min(snamlen, DTSLOTDATALEN);
			UniStrncpy(d->name, s->name, len);

			ns++;
			nd++;
			xssi = ssi;

			dsi++;
			d->next = dsi;

			/* free source segment */
			next = s->next;
			s->next = sfsi;
			s->cnt = 1;
			sfsi = ssi;

			snamlen -= len;
		}		/* end while */

		/* terminate dst last/only segment */
		if (h == d) {
			/* single segment entry */
			if (dp->header.flag & BT_LEAF)
				dlh->next = -1;
			else
				dih->next = -1;
		} else
			/* multi-segment entry */
			d->next = -1;
	}			/* end for */

	/* close current linelock */
	slv->length = ns;
	sdtlck->index++;
	*sdtlock = sdtlck;

	dlv->length = nd;
	ddtlck->index++;
	*ddtlock = ddtlck;

	/* update source header */
	sp->header.freelist = sfsi;
	sp->header.freecnt += nd;

	/* update destination header */
	dp->header.nextindex = di;

	dp->header.freelist = dsi;
	dp->header.freecnt -= nd;
}


/*
 *	dtDeleteEntry()
 *
 * function: free a (leaf/internal) entry
 *
 * log freelist header, stbl, and each segment slot of entry
 * (even though last/only segment next field is modified,
 * physical image logging requires all segment slots of
 * the entry logged to avoid applying previous updates
 * to the same slots)
 */
static void dtDeleteEntry(dtpage_t * p, int fi, dtlock_t ** dtlock)
{
	int fsi;		/* free entry slot index */
	s8 *stbl;
	dtslot_t *t;
	int si, freecnt;
	dtlock_t *dtlck = *dtlock;
	lv_t *lv;
	int xsi, n;

	/* get free entry slot index */
	stbl = DT_GETSTBL(p);
	fsi = stbl[fi];

	/* open new linelock */
	if (dtlck->index >= dtlck->maxcnt)
		dtlck = (dtlock_t *) txLinelock(dtlck);
	lv = (lv_t *) & dtlck->lv[dtlck->index];

	lv->offset = fsi;

	/* get the head/only segment */
	t = &p->slot[fsi];
	if (p->header.flag & BT_LEAF)
		si = ((ldtentry_t *) t)->next;
	else
		si = ((idtentry_t *) t)->next;
	t->next = si;
	t->cnt = 1;

	n = freecnt = 1;
	xsi = fsi;

	/* find the last/only segment */
	while (si >= 0) {
		/* is next slot contiguous ? */
		if (si != xsi + 1) {
			/* close current linelock */
			lv->length = n;
			dtlck->index++;

			/* open new linelock */
			if (dtlck->index < dtlck->maxcnt)
				lv++;
			else {
				dtlck = (dtlock_t *) txLinelock(dtlck);
				lv = (lv_t *) & dtlck->lv[0];
			}

			lv->offset = si;
			n = 0;
		}

		n++;
		xsi = si;
		freecnt++;

		t = &p->slot[si];
		t->cnt = 1;
		si = t->next;
	}

	/* close current linelock */
	lv->length = n;
	dtlck->index++;

	*dtlock = dtlck;

	/* update freelist */
	t->next = p->header.freelist;
	p->header.freelist = fsi;
	p->header.freecnt += freecnt;

	/* if delete from middle,
	 * shift left the succedding entries in the stbl
	 */
	si = p->header.nextindex;
	if (fi < si - 1)
		memmove(&stbl[fi], &stbl[fi + 1], si - fi - 1);

	p->header.nextindex--;
}


/*
 *	dtTruncateEntry()
 *
 * function: truncate a (leaf/internal) entry
 *
 * log freelist header, stbl, and each segment slot of entry
 * (even though last/only segment next field is modified,
 * physical image logging requires all segment slots of
 * the entry logged to avoid applying previous updates
 * to the same slots)
 */
static void dtTruncateEntry(dtpage_t * p, int ti, dtlock_t ** dtlock)
{
	int tsi;		/* truncate entry slot index */
	s8 *stbl;
	dtslot_t *t;
	int si, freecnt;
	dtlock_t *dtlck = *dtlock;
	lv_t *lv;
	int fsi, xsi, n;

	/* get free entry slot index */
	stbl = DT_GETSTBL(p);
	tsi = stbl[ti];

	/* open new linelock */
	if (dtlck->index >= dtlck->maxcnt)
		dtlck = (dtlock_t *) txLinelock(dtlck);
	lv = (lv_t *) & dtlck->lv[dtlck->index];

	lv->offset = tsi;

	/* get the head/only segment */
	t = &p->slot[tsi];
	ASSERT(p->header.flag & BT_INTERNAL);
	((idtentry_t *) t)->namlen = 0;
	si = ((idtentry_t *) t)->next;
	((idtentry_t *) t)->next = -1;

	n = 1;
	freecnt = 0;
	fsi = si;
	xsi = tsi;

	/* find the last/only segment */
	while (si >= 0) {
		/* is next slot contiguous ? */
		if (si != xsi + 1) {
			/* close current linelock */
			lv->length = n;
			dtlck->index++;

			/* open new linelock */
			if (dtlck->index < dtlck->maxcnt)
				lv++;
			else {
				dtlck = (dtlock_t *) txLinelock(dtlck);
				lv = (lv_t *) & dtlck->lv[0];
			}

			lv->offset = si;
			n = 0;
		}

		n++;
		xsi = si;
		freecnt++;

		t = &p->slot[si];
		t->cnt = 1;
		si = t->next;
	}

	/* close current linelock */
	lv->length = n;
	dtlck->index++;

	*dtlock = dtlck;

	/* update freelist */
	if (freecnt == 0)
		return;
	t->next = p->header.freelist;
	p->header.freelist = fsi;
	p->header.freecnt += freecnt;
}


/*
 *	dtLinelockFreelist()
 */
static void dtLinelockFreelist(dtpage_t * p,	/* directory page */
			       int m,	/* max slot index */
			       dtlock_t ** dtlock)
{
	int fsi;		/* free entry slot index */
	dtslot_t *t;
	int si;
	dtlock_t *dtlck = *dtlock;
	lv_t *lv;
	int xsi, n;

	/* get free entry slot index */
	fsi = p->header.freelist;

	/* open new linelock */
	if (dtlck->index >= dtlck->maxcnt)
		dtlck = (dtlock_t *) txLinelock(dtlck);
	lv = (lv_t *) & dtlck->lv[dtlck->index];

	lv->offset = fsi;

	n = 1;
	xsi = fsi;

	t = &p->slot[fsi];
	si = t->next;

	/* find the last/only segment */
	while (si < m && si >= 0) {
		/* is next slot contiguous ? */
		if (si != xsi + 1) {
			/* close current linelock */
			lv->length = n;
			dtlck->index++;

			/* open new linelock */
			if (dtlck->index < dtlck->maxcnt)
				lv++;
			else {
				dtlck = (dtlock_t *) txLinelock(dtlck);
				lv = (lv_t *) & dtlck->lv[0];
			}

			lv->offset = si;
			n = 0;
		}

		n++;
		xsi = si;

		t = &p->slot[si];
		si = t->next;
	}

	/* close current linelock */
	lv->length = n;
	dtlck->index++;

	*dtlock = dtlck;
}


/*
 * NAME: dtModify
 *
 * FUNCTION: Modify the inode number part of a directory entry
 *
 * PARAMETERS:
 *	tid	- Transaction id
 *	ip	- Inode of parent directory
 *	key	- Name of entry to be modified
 *	orig_ino	- Original inode number expected in entry
 *	new_ino	- New inode number to put into entry
 *	flag	- JFS_RENAME
 *
 * RETURNS:
 *	ESTALE	- If entry found does not match orig_ino passed in
 *	ENOENT	- If no entry can be found to match key
 *	0	- If successfully modified entry
 */
int dtModify(tid_t tid, struct inode *ip,
	 component_t * key, ino_t * orig_ino, ino_t new_ino, int flag)
{
	int rc;
	s64 bn;
	metapage_t *mp;
	dtpage_t *p;
	int index;
	btstack_t btstack;
	tlock_t *tlck;
	dtlock_t *dtlck;
	lv_t *lv;
	s8 *stbl;
	int entry_si;		/* entry slot index */
	ldtentry_t *entry;

	/*
	 *      search for the entry to modify:
	 *
	 * dtSearch() returns (leaf page pinned, index at which to modify).
	 */
	if ((rc = dtSearch(ip, key, orig_ino, &btstack, flag)))
		return rc;

	/* retrieve search result */
	DT_GETSEARCH(ip, btstack.top, bn, mp, p, index);

	BT_MARK_DIRTY(mp, ip);
	/*
	 * acquire a transaction lock on the leaf page of named entry
	 */
	tlck = txLock(tid, ip, mp, tlckDTREE | tlckENTRY);
	dtlck = (dtlock_t *) & tlck->lock;

	/* get slot index of the entry */
	stbl = DT_GETSTBL(p);
	entry_si = stbl[index];

	/* linelock entry */
	ASSERT(dtlck->index == 0);
	lv = (lv_t *) & dtlck->lv[0];
	lv->offset = entry_si;
	lv->length = 1;
	dtlck->index++;

	/* get the head/only segment */
	entry = (ldtentry_t *) & p->slot[entry_si];

	/* substitute the inode number of the entry */
	entry->inumber = cpu_to_le32(new_ino);

	/* unpin the leaf page */
	DT_PUTPAGE(mp);

	return 0;
}

#ifdef _JFS_DEBUG_DTREE
/*
 *	dtDisplayTree()
 *
 * function: traverse forward
 */
int dtDisplayTree(struct inode *ip)
{
	int rc;
	metapage_t *mp;
	dtpage_t *p;
	s64 bn, pbn;
	int index, lastindex, v, h;
	pxd_t *xd;
	btstack_t btstack;
	btframe_t *btsp;
	btframe_t *parent;
	u8 *stbl;
	int psize = 256;

	printk("display B+-tree.\n");

	/* clear stack */
	btsp = btstack.stack;

	/*
	 * start with root
	 *
	 * root resides in the inode
	 */
	bn = 0;
	v = h = 0;

	/*
	 * first access of each page:
	 */
      newPage:
	DT_GETPAGE(ip, bn, mp, psize, p, rc);
	if (rc)
		return rc;

	/* process entries forward from first index */
	index = 0;
	lastindex = p->header.nextindex - 1;

	if (p->header.flag & BT_INTERNAL) {
		/*
		 * first access of each internal page
		 */
		printf("internal page ");
		dtDisplayPage(ip, bn, p);

		goto getChild;
	} else {		/* (p->header.flag & BT_LEAF) */

		/*
		 * first access of each leaf page
		 */
		printf("leaf page ");
		dtDisplayPage(ip, bn, p);

		/*
		 * process leaf page entries
		 *
		 for ( ; index <= lastindex; index++)
		 {
		 }
		 */

		/* unpin the leaf page */
		DT_PUTPAGE(mp);
	}

	/*
	 * go back up to the parent page
	 */
      getParent:
	/* pop/restore parent entry for the current child page */
	if ((parent = (btsp == btstack.stack ? NULL : --btsp)) == NULL)
		/* current page must have been root */
		return;

	/*
	 * parent page scan completed
	 */
	if ((index = parent->index) == (lastindex = parent->lastindex)) {
		/* go back up to the parent page */
		goto getParent;
	}

	/*
	 * parent page has entries remaining
	 */
	/* get back the parent page */
	bn = parent->bn;
	/* v = parent->level; */
	DT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
	if (rc)
		return rc;

	/* get next parent entry */
	index++;

	/*
	 * internal page: go down to child page of current entry
	 */
      getChild:
	/* push/save current parent entry for the child page */
	btsp->bn = pbn = bn;
	btsp->index = index;
	btsp->lastindex = lastindex;
	/* btsp->level = v; */
	/* btsp->node = h; */
	++btsp;

	/* get current entry for the child page */
	stbl = DT_GETSTBL(p);
	xd = (pxd_t *) & p->slot[stbl[index]];

	/*
	 * first access of each internal entry:
	 */

	/* get child page */
	bn = addressPXD(xd);
	psize = lengthPXD(xd) << ip->i_ipmnt->i_l2bsize;

	printk("traverse down 0x%Lx[%d]->0x%Lx\n", pbn, index, bn);
	v++;
	h = index;

	/* release parent page */
	DT_PUTPAGE(mp);

	/* process the child page */
	goto newPage;
}


/*
 *	dtDisplayPage()
 *
 * function: display page
 */
int dtDisplayPage(struct inode *ip, s64 bn, dtpage_t * p)
{
	int rc;
	metapage_t *mp;
	ldtentry_t *lh;
	idtentry_t *ih;
	pxd_t *xd;
	int i, j;
	u8 *stbl;
	wchar_t name[JFS_NAME_MAX + 1];
	component_t key = { 0, name };
	int freepage = 0;

	if (p == NULL) {
		freepage = 1;
		DT_GETPAGE(ip, bn, mp, PSIZE, p, rc);
		if (rc)
			return rc;
	}

	/* display page control */
	printk("bn:0x%Lx flag:0x%08x nextindex:%d\n",
	       bn, p->header.flag, p->header.nextindex);

	/* display entries */
	stbl = DT_GETSTBL(p);
	for (i = 0, j = 1; i < p->header.nextindex; i++, j++) {
		dtGetKey(p, i, &key, JFS_SBI(ip->i_sb)->mntflag);
		key.name[key.namlen] = '\0';
		if (p->header.flag & BT_LEAF) {
			lh = (ldtentry_t *) & p->slot[stbl[i]];
			printf("\t[%d] %s:%d", i, key.name,
			       le32_to_cpu(lh->inumber));
		} else {
			ih = (idtentry_t *) & p->slot[stbl[i]];
			xd = (pxd_t *) ih;
			bn = addressPXD(xd);
			printf("\t[%d] %s:0x%Lx", i, key.name, bn);
		}

		if (j == 4) {
			printf("\n");
			j = 0;
		}
	}

	printf("\n");

	if (freepage)
		DT_PUTPAGE(mp);

	return 0;
}
#endif				/* _JFS_DEBUG_DTREE */
