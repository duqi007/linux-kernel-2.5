/*
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright (C) 2001, 2002 Red Hat, Inc.
 *
 * Created by David Woodhouse <dwmw2@cambridge.redhat.com>
 *
 * The original JFFS, from which the design for JFFS2 was derived,
 * was designed and implemented by Axis Communications AB.
 *
 * The contents of this file are subject to the Red Hat eCos Public
 * License Version 1.1 (the "Licence"); you may not use this file
 * except in compliance with the Licence.  You may obtain a copy of
 * the Licence at http://www.redhat.com/
 *
 * Software distributed under the Licence is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.
 * See the Licence for the specific language governing rights and
 * limitations under the Licence.
 *
 * The Original Code is JFFS2 - Journalling Flash File System, version 2
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License version 2 (the "GPL"), in
 * which case the provisions of the GPL are applicable instead of the
 * above.  If you wish to allow the use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the RHEPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GPL.  If you do not delete the
 * provisions above, a recipient may use your version of this file
 * under either the RHEPL or the GPL.
 *
 * $Id: super.c,v 1.64 2002/03/17 10:18:42 dwmw2 Exp $
 *
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/fs.h>
#include <linux/jffs2.h>
#include <linux/pagemap.h>
#include <linux/mtd/mtd.h>
#include <linux/interrupt.h>
#include <linux/ctype.h>
#include "nodelist.h"

void jffs2_put_super (struct super_block *);


static kmem_cache_t *jffs2_inode_cachep;

static struct inode *jffs2_alloc_inode(struct super_block *sb)
{
	struct jffs2_inode_info *ei;
	ei = (struct jffs2_inode_info *)kmem_cache_alloc(jffs2_inode_cachep, SLAB_KERNEL);
	if (!ei)
		return NULL;
	return &ei->vfs_inode;
}

static void jffs2_destroy_inode(struct inode *inode)
{
	kmem_cache_free(jffs2_inode_cachep, JFFS2_INODE_INFO(inode));
}

static void jffs2_i_init_once(void * foo, kmem_cache_t * cachep, unsigned long flags)
{
	struct jffs2_inode_info *ei = (struct jffs2_inode_info *) foo;

	if ((flags & (SLAB_CTOR_VERIFY|SLAB_CTOR_CONSTRUCTOR)) ==
	    SLAB_CTOR_CONSTRUCTOR) {
		init_MUTEX(&ei->sem);
		inode_init_once(&ei->vfs_inode);
	}
}

static struct super_operations jffs2_super_operations =
{
	alloc_inode:	jffs2_alloc_inode,
	destroy_inode:	jffs2_destroy_inode,
	read_inode:	jffs2_read_inode,
	put_super:	jffs2_put_super,
	write_super:	jffs2_write_super,
	statfs:		jffs2_statfs,
	remount_fs:	jffs2_remount_fs,
	clear_inode:	jffs2_clear_inode
};

static int jffs2_sb_compare(struct super_block *sb, void *data)
{
	struct jffs2_sb_info *p = data;
	struct jffs2_sb_info *c = JFFS2_SB_INFO(sb);

	/* The superblocks are considered to be equivalent if the underlying MTD
	   device is the same one */
	if (c->mtd == p->mtd) {
		D1(printk(KERN_DEBUG "jffs2_sb_compare: match on device %d (\"%s\")\n", mtd->index, mtd->name));
		return 1;
	} else {
		D1(printk(KERN_DEBUG "jffs2_sb_compare: No match, device %d (\"%s\"), device %d (\"%s\")\n",
			  c->mtd->index, c->mtd->name, mtd->index, mtd->name));
		return 0;
	}
}

static int jffs2_sb_set(struct super_block *sb, void *data)
{
	struct jffs2_sb_info *p = data;

	/* For persistence of NFS exports etc. we use the same s_dev
	   each time we mount the device, don't just use an anonymous
	   device */
	sb->u.generic_sbp = p;
	p->os_priv = sb;
	sb->s_dev = mk_kdev(MTD_BLOCK_MAJOR, p->mtd->index);

	return 0;
}

static struct super_block *jffs2_get_sb_mtd(struct file_system_type *fs_type,
					      int flags, char *dev_name, 
					      void *data, struct mtd_info *mtd)
{
	struct super_block *sb;
	struct jffs2_sb_info *c;
	int ret;

	c = kmalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return ERR_PTR(-ENOMEM);
	memset(c, 0, sizeof(*c));
	c->mtd = mtd;

	sb = sget(fs_type, jffs2_sb_compare, jffs2_sb_set, c);

	if (IS_ERR(sb))
		goto out_put;

	if (sb->s_root) {
		/* New mountpoint for JFFS2 which is already mounted */
		D1(printk(KERN_DEBUG "jffs2_get_sb_mtd(): Device %d (\"%s\") is already mounted\n",
			  mtd->index, mtd->name));
		goto out_put;
	}

	D1(printk(KERN_DEBUG "jffs2_get_sb_mtd(): New superblock for device %d (\"%s\")\n",
		  mtd->index, mtd->name));

	sb->s_op = &jffs2_super_operations;

	ret = jffs2_do_fill_super(sb, data, (flags&MS_VERBOSE)?1:0);

	if (ret) {
		/* Failure case... */
		up_write(&sb->s_umount);
		deactivate_super(sb);
		sb = ERR_PTR(ret);
		goto out_put1;
	}

	sb->s_flags |= MS_ACTIVE;
	return sb;

 out_put:
	kfree(c);
 out_put1:
	put_mtd_device(mtd);

	return sb;
}

static struct super_block *jffs2_get_sb_mtdnr(struct file_system_type *fs_type,
					      int flags, char *dev_name, 
					      void *data, int mtdnr)
{
	struct mtd_info *mtd;

	mtd = get_mtd_device(NULL, mtdnr);
	if (!mtd) {
		D1(printk(KERN_DEBUG "jffs2: MTD device #%u doesn't appear to exist\n", mtdnr));
		return ERR_PTR(-EINVAL);
	}

	return jffs2_get_sb_mtd(fs_type, flags, dev_name, data, mtd);
}

static struct super_block *jffs2_get_sb(struct file_system_type *fs_type,
					int flags, char *dev_name, void *data)
{
	int err;
	struct nameidata nd;
	int mtdnr;
	kdev_t dev;

	if (!dev_name)
		return ERR_PTR(-EINVAL);

	D1(printk(KERN_DEBUG "jffs2_get_sb(): dev_name \"%s\"\n", dev_name));

	/* The preferred way of mounting in future; especially when
	   CONFIG_BLK_DEV is implemented - we specify the underlying
	   MTD device by number or by name, so that we don't require 
	   block device support to be present in the kernel. */

	/* FIXME: How to do the root fs this way? */

	if (dev_name[0] == 'm' && dev_name[1] == 't' && dev_name[2] == 'd') {
		/* Probably mounting without the blkdev crap */
		if (dev_name[3] == ':') {
			struct mtd_info *mtd;

			/* Mount by MTD device name */
			D1(printk(KERN_DEBUG "jffs2_get_sb(): mtd:%%s, name \"%s\"\n", dev_name+4));
			for (mtdnr = 0; mtdnr < MAX_MTD_DEVICES; mtdnr++) {
				mtd = get_mtd_device(NULL, mtdnr);
				if (mtd) {
					if (!strcmp(mtd->name, dev_name+4))
						return jffs2_get_sb_mtd(fs_type, flags, dev_name, data, mtd);
					put_mtd_device(mtd);
				}
			}
			printk(KERN_NOTICE "jffs2_get_sb(): MTD device with name \"%s\" not found.\n", dev_name+4);
		} else if (isdigit(dev_name[3])) {
			/* Mount by MTD device number name */
			char *endptr;
			
			mtdnr = simple_strtoul(dev_name+3, &endptr, 0);
			if (!*endptr) {
				/* It was a valid number */
				D1(printk(KERN_DEBUG "jffs2_get_sb(): mtd%%d, mtdnr %d\n", mtdnr));
				return jffs2_get_sb_mtdnr(fs_type, flags, dev_name, data, mtdnr);
			}
		}
	}

	/* Try the old way - the hack where we allowed users to mount 
	   /dev/mtdblock$(n) but didn't actually _use_ the blkdev */

	err = path_lookup(dev_name, LOOKUP_FOLLOW, &nd);

	D1(printk(KERN_DEBUG "jffs2_get_sb(): path_lookup() returned %d, inode %p\n",
		  err, nd.dentry->d_inode));

	if (err)
		return ERR_PTR(err);

	if (!S_ISBLK(nd.dentry->d_inode->i_mode)) {
		path_release(&nd);
		return ERR_PTR(-EINVAL);
	}
	if (nd.mnt->mnt_flags & MNT_NODEV) {
		path_release(&nd);
		return ERR_PTR(-EACCES);
	}

	dev = nd.dentry->d_inode->i_rdev;
	path_release(&nd);

	if (major(dev) != MTD_BLOCK_MAJOR) {
		if (!(flags & MS_VERBOSE)) /* Yes I mean this. Strangely */
			printk(KERN_NOTICE "Attempt to mount non-MTD device \"%s\" as JFFS2\n",
			       dev_name);
		return ERR_PTR(-EINVAL);
	}

	return jffs2_get_sb_mtdnr(fs_type, flags, dev_name, data, minor(dev));
}


void jffs2_put_super (struct super_block *sb)
{
	struct jffs2_sb_info *c = JFFS2_SB_INFO(sb);

	D2(printk(KERN_DEBUG "jffs2: jffs2_put_super()\n"));

	if (!(sb->s_flags & MS_RDONLY))
		jffs2_stop_garbage_collect_thread(c);
	jffs2_flush_wbuf(c, 1);
	jffs2_free_ino_caches(c);
	jffs2_free_raw_node_refs(c);
	kfree(c->blocks);
	if (c->mtd->sync)
		c->mtd->sync(c->mtd);

	D1(printk(KERN_DEBUG "jffs2_put_super returning\n"));
}

static void jffs2_kill_sb(struct super_block *sb)
{
	struct jffs2_sb_info *c = JFFS2_SB_INFO(sb);
	generic_shutdown_super(sb);
	put_mtd_device(c->mtd);
	kfree(c);
}
 
static struct file_system_type jffs2_fs_type = {
	owner:		THIS_MODULE,
	name:		"jffs2",
	get_sb:		jffs2_get_sb,
	kill_sb:	jffs2_kill_sb,
};



static int __init init_jffs2_fs(void)
{
	int ret;

	printk(KERN_INFO "JFFS2 version 2.1. (C) 2001, 2002 Red Hat, Inc.\n");

	jffs2_inode_cachep = kmem_cache_create("jffs2_i",
					     sizeof(struct jffs2_inode_info),
					     0, SLAB_HWCACHE_ALIGN,
					     jffs2_i_init_once, NULL);
	if (!jffs2_inode_cachep) {
		printk(KERN_ERR "JFFS2 error: Failed to initialise inode cache\n");
		return -ENOMEM;
	}
	ret = jffs2_zlib_init();
	if (ret) {
		printk(KERN_ERR "JFFS2 error: Failed to initialise zlib workspaces\n");
		return ret;
	}
	ret = jffs2_create_slab_caches();
	if (ret) {
		printk(KERN_ERR "JFFS2 error: Failed to initialise slab caches\n");
		return ret;
	}
	ret = register_filesystem(&jffs2_fs_type);
	if (ret) {
		printk(KERN_ERR "JFFS2 error: Failed to register filesystem\n");
		jffs2_destroy_slab_caches();
	}
	return ret;
}

static void __exit exit_jffs2_fs(void)
{
	unregister_filesystem(&jffs2_fs_type);
	jffs2_destroy_slab_caches();
	jffs2_zlib_exit();
	kmem_cache_destroy(jffs2_inode_cachep);
}

module_init(init_jffs2_fs);
module_exit(exit_jffs2_fs);

MODULE_DESCRIPTION("The Journalling Flash File System, v2");
MODULE_AUTHOR("Red Hat, Inc.");
MODULE_LICENSE("GPL"); // Actually dual-licensed, but it doesn't matter for 
		       // the sake of this tag. It's Free Software.