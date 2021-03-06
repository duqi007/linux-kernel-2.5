/*
 *  Copyright (C) 1994-1998  Linus Torvalds and authors:
 *
 *	Mark Lord <mlord@pobox.com>
 *	Gadi Oxman <gadio@netvision.net.il>
 *	Andre Hedrick <andre@linux-ide.org>
 *	Jens Axboe <axboe@suse.de>
 *	Marcin Dalecki <dalecki@evision.ag>
 *
 * This is the ATA disk device driver, as evolved from hd.c and ide.c.
 */

#define IDEDISK_VERSION	"1.14"

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/major.h>
#include <linux/errno.h>
#include <linux/genhd.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/ide.h>

#include <asm/byteorder.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#ifdef CONFIG_BLK_DEV_PDC4030
#define IS_PDC4030_DRIVE (drive->channel->chipset == ide_pdc4030)
#else
#define IS_PDC4030_DRIVE (0)	/* auto-NULLs out pdc4030 code */
#endif

/*
 * Perform a sanity check on the claimed "lba_capacity"
 * value for this drive (from its reported identification information).
 *
 * Returns:	1 if lba_capacity looks sensible
 *		0 otherwise
 *
 * It is called only once for each drive.
 */
static int lba_capacity_is_ok(struct hd_driveid *id)
{
	unsigned long lba_sects, chs_sects, head, tail;

	if ((id->command_set_2 & 0x0400) && (id->cfs_enable_2 & 0x0400)) {
		printk("48-bit Drive: %llu \n", id->lba_capacity_2);
		return 1;
	}

	/*
	 * The ATA spec tells large drives to return
	 * C/H/S = 16383/16/63 independent of their size.
	 * Some drives can be jumpered to use 15 heads instead of 16.
	 * Some drives can be jumpered to use 4092 cyls instead of 16383.
	 */
	if ((id->cyls == 16383
	     || (id->cyls == 4092 && id->cur_cyls == 16383)) &&
	    id->sectors == 63 &&
	    (id->heads == 15 || id->heads == 16) &&
	    id->lba_capacity >= 16383*63*id->heads)
		return 1;

	lba_sects   = id->lba_capacity;
	chs_sects   = id->cyls * id->heads * id->sectors;

	/* perform a rough sanity check on lba_sects:  within 10% is OK */
	if ((lba_sects - chs_sects) < chs_sects/10)
		return 1;

	/* some drives have the word order reversed */
	head = ((lba_sects >> 16) & 0xffff);
	tail = (lba_sects & 0xffff);
	lba_sects = (head | (tail << 16));
	if ((lba_sects - chs_sects) < chs_sects/10) {
		id->lba_capacity = lba_sects;
		return 1;	/* lba_capacity is (now) good */
	}

	return 0;	/* lba_capacity value may be bad */
}

static u8 get_command(ide_drive_t *drive, int cmd)
{
	int lba48bit = (drive->id->cfs_enable_2 & 0x0400) ? 1 : 0;

#if 1
	lba48bit = drive->addressing;
#endif

	if (lba48bit) {
		if (cmd == READ) {
			if (drive->using_dma)
				return WIN_READDMA_EXT;
			else if (drive->mult_count)
				return WIN_MULTREAD_EXT;
			else
				return WIN_READ_EXT;
		} else if (cmd == WRITE) {
			if (drive->using_dma)
				return WIN_WRITEDMA_EXT;
			else if (drive->mult_count)
				return WIN_MULTWRITE_EXT;
			else
				return WIN_WRITE_EXT;
		}
	} else {
		if (cmd == READ) {
			if (drive->using_dma)
				return WIN_READDMA;
			else if (drive->mult_count)
				return WIN_MULTREAD;
			else
				return WIN_READ;
		} else if (cmd == WRITE) {
			if (drive->using_dma)
				return WIN_WRITEDMA;
			else if (drive->mult_count)
				return WIN_MULTWRITE;
			else
				return WIN_WRITE;
		}
	}
	return WIN_NOP;
}

static ide_startstop_t chs_do_request(struct ata_device *drive, struct request *rq, sector_t block)
{
	struct ata_taskfile args;
	int sectors;

	unsigned int track	= (block / drive->sect);
	unsigned int sect	= (block % drive->sect) + 1;
	unsigned int head	= (track % drive->head);
	unsigned int cyl	= (track / drive->head);

	sectors = rq->nr_sectors;
	if (sectors == 256)
		sectors = 0;

	memset(&args, 0, sizeof(args));

	args.taskfile.sector_count = sectors;

	args.taskfile.sector_number = sect;
	args.taskfile.low_cylinder = cyl;
	args.taskfile.high_cylinder = (cyl>>8);

	args.taskfile.device_head = head;
	args.taskfile.device_head |= drive->select.all;
	args.taskfile.command =  get_command(drive, rq_data_dir(rq));

#ifdef DEBUG
	printk("%s: %sing: ", drive->name,
		(rq_data_dir(rq)==READ) ? "read" : "writ");
	if (lba)	printk("LBAsect=%lld, ", block);
	else		printk("CHS=%d/%d/%d, ", cyl, head, sect);
	printk("sectors=%ld, ", rq->nr_sectors);
	printk("buffer=%p\n", rq->buffer);
#endif

	ide_cmd_type_parser(&args);
	rq->special = &args;

	return ata_taskfile(drive, &args, rq);
}

static ide_startstop_t lba28_do_request(struct ata_device *drive, struct request *rq, sector_t block)
{
	struct ata_taskfile args;
	int sectors;

	sectors = rq->nr_sectors;
	if (sectors == 256)
		sectors = 0;

	memset(&args, 0, sizeof(args));

	args.taskfile.sector_count = sectors;
	args.taskfile.sector_number = block;
	args.taskfile.low_cylinder = (block >>= 8);

	args.taskfile.high_cylinder = (block >>= 8);

	args.taskfile.device_head = ((block >> 8) & 0x0f);
	args.taskfile.device_head |= drive->select.all;
	args.taskfile.command = get_command(drive, rq_data_dir(rq));

#ifdef DEBUG
	printk("%s: %sing: ", drive->name,
		(rq_data_dir(rq)==READ) ? "read" : "writ");
	if (lba)	printk("LBAsect=%lld, ", block);
	else		printk("CHS=%d/%d/%d, ", cyl, head, sect);
	printk("sectors=%ld, ", rq->nr_sectors);
	printk("buffer=%p\n", rq->buffer);
#endif

	ide_cmd_type_parser(&args);
	rq->special = &args;

	return ata_taskfile(drive, &args, rq);
}

/*
 * 268435455  == 137439 MB or 28bit limit
 * 320173056  == 163929 MB or 48bit addressing
 * 1073741822 == 549756 MB or 48bit addressing fake drive
 */

static ide_startstop_t lba48_do_request(struct ata_device *drive, struct request *rq, sector_t block)
{
	struct ata_taskfile args;
	int sectors;

	sectors = rq->nr_sectors;
	if (sectors == 65536)
		sectors = 0;

	memset(&args, 0, sizeof(args));

	args.taskfile.sector_count = sectors;
	args.hobfile.sector_count = sectors >> 8;

	args.taskfile.sector_number = block;		/* low lba */
	args.taskfile.low_cylinder = (block >>= 8);	/* mid lba */
	args.taskfile.high_cylinder = (block >>= 8);	/* hi  lba */

	args.hobfile.sector_number = (block >>= 8);	/* low lba */
	args.hobfile.low_cylinder = (block >>= 8);	/* mid lba */
	args.hobfile.high_cylinder = (block >>= 8);	/* hi  lba */

	args.taskfile.device_head = drive->select.all;
	args.hobfile.device_head = args.taskfile.device_head;
	args.hobfile.control = (drive->ctl|0x80);
	args.taskfile.command = get_command(drive, rq_data_dir(rq));

#ifdef DEBUG
	printk("%s: %sing: ", drive->name,
		(rq_data_dir(rq)==READ) ? "read" : "writ");
	if (lba)	printk("LBAsect=%lld, ", block);
	else		printk("CHS=%d/%d/%d, ", cyl, head, sect);
	printk("sectors=%ld, ", rq->nr_sectors);
	printk("buffer=%p\n",rq->buffer);
#endif

	ide_cmd_type_parser(&args);
	rq->special = &args;

	return ata_taskfile(drive, &args, rq);
}

/*
 * Issue a READ or WRITE command to a disk, using LBA if supported, or CHS
 * otherwise, to address sectors.  It also takes care of issuing special
 * DRIVE_CMDs.
 */
static ide_startstop_t idedisk_do_request(struct ata_device *drive, struct request *rq, sector_t block)
{
	/*
	 * Wait until all request have bin finished.
	 */

	while (drive->blocked) {
		yield();
		printk(KERN_ERR "ide: Request while drive blocked?");
	}

	if (!(rq->flags & REQ_CMD)) {
		blk_dump_rq_flags(rq, "idedisk_do_request - bad command");
		ide_end_request(drive, rq, 0);
		return ide_stopped;
	}

	if (IS_PDC4030_DRIVE) {
		extern ide_startstop_t promise_rw_disk(struct ata_device *, struct request *, unsigned long);

		return promise_rw_disk(drive, rq, block);
	}

	/* 48-bit LBA */
	if ((drive->id->cfs_enable_2 & 0x0400) && (drive->addressing))
		return lba48_do_request(drive, rq, block);

	/* 28-bit LBA */
	if (drive->select.b.lba)
		return lba28_do_request(drive, rq, block);

	/* 28-bit CHS */
	return chs_do_request(drive, rq, block);
}

static int idedisk_open (struct inode *inode, struct file *filp, ide_drive_t *drive)
{
	MOD_INC_USE_COUNT;
	if (drive->removable && drive->usage == 1) {
		struct ata_taskfile args;

		check_disk_change(inode->i_rdev);

		memset(&args, 0, sizeof(args));

		args.taskfile.command = WIN_DOORLOCK;
		ide_cmd_type_parser(&args);

		/*
		 * Ignore the return code from door_lock, since the open() has
		 * already succeeded, and the door_lock is irrelevant at this
		 * point.
		 */

		if (drive->doorlocking && ide_raw_taskfile(drive, &args, NULL))
			drive->doorlocking = 0;
	}
	return 0;
}

static int idedisk_flushcache(ide_drive_t *drive)
{
	struct ata_taskfile args;

	memset(&args, 0, sizeof(args));

	if (drive->id->cfs_enable_2 & 0x2400)
		args.taskfile.command = WIN_FLUSH_CACHE_EXT;
	else
		args.taskfile.command = WIN_FLUSH_CACHE;

	ide_cmd_type_parser(&args);

	return ide_raw_taskfile(drive, &args, NULL);
}

static void idedisk_release (struct inode *inode, struct file *filp, ide_drive_t *drive)
{
	if (drive->removable && !drive->usage) {
		struct ata_taskfile args;

		invalidate_bdev(inode->i_bdev, 0);

		memset(&args, 0, sizeof(args));
		args.taskfile.command = WIN_DOORUNLOCK;
		ide_cmd_type_parser(&args);

		if (drive->doorlocking &&
		    ide_raw_taskfile(drive, &args, NULL))
			drive->doorlocking = 0;
	}
	if ((drive->id->cfs_enable_2 & 0x3000) && drive->wcache)
		if (idedisk_flushcache(drive))
			printk (KERN_INFO "%s: Write Cache FAILED Flushing!\n",
				drive->name);
	MOD_DEC_USE_COUNT;
}

static int idedisk_check_media_change (ide_drive_t *drive)
{
	/* if removable, always assume it was changed */
	return drive->removable;
}

static sector_t idedisk_capacity(struct ata_device *drive)
{
	return drive->capacity - drive->sect0;
}

static ide_startstop_t idedisk_special(struct ata_device *drive)
{
	unsigned char special_cmd = drive->special_cmd;

	if (special_cmd & ATA_SPECIAL_GEOMETRY) {
		struct ata_taskfile args;

		drive->special_cmd &= ~ATA_SPECIAL_GEOMETRY;

		memset(&args, 0, sizeof(args));
		args.taskfile.sector_number	= drive->sect;
		args.taskfile.low_cylinder	= drive->cyl;
		args.taskfile.high_cylinder	= drive->cyl>>8;
		args.taskfile.device_head	= ((drive->head-1)|drive->select.all)&0xBF;
		if (!IS_PDC4030_DRIVE) {
			args.taskfile.sector_count = drive->sect;
			args.taskfile.command = WIN_SPECIFY;
			args.handler = set_geometry_intr;;
		}
		ata_taskfile(drive, &args, NULL);
	} else if (special_cmd & ATA_SPECIAL_RECALIBRATE) {
		drive->special_cmd &= ~ATA_SPECIAL_RECALIBRATE;

		if (!IS_PDC4030_DRIVE) {
			struct ata_taskfile args;

			memset(&args, 0, sizeof(args));
			args.taskfile.sector_count = drive->sect;
			args.taskfile.command = WIN_RESTORE;
			args.handler = recal_intr;
			ata_taskfile(drive, &args, NULL);
		}
	} else if (special_cmd & ATA_SPECIAL_MMODE) {
		drive->special_cmd &= ~ATA_SPECIAL_MMODE;
		if (drive->id && drive->mult_req > drive->id->max_multsect)
			drive->mult_req = drive->id->max_multsect;
		if (!IS_PDC4030_DRIVE) {
			struct ata_taskfile args;

			memset(&args, 0, sizeof(args));
			args.taskfile.sector_count = drive->mult_req;
			args.taskfile.command = WIN_SETMULT;
			args.handler = set_multmode_intr;

			ata_taskfile(drive, &args, NULL);
		}
	} else if (special_cmd) {
		drive->special_cmd = 0;

		printk(KERN_ERR "%s: bad special flag: 0x%02x\n", drive->name, special_cmd);
		return ide_stopped;
	}
	return IS_PDC4030_DRIVE ? ide_stopped : ide_started;
}

static void idedisk_pre_reset (ide_drive_t *drive)
{
	int legacy = (drive->id->cfs_enable_2 & 0x0400) ? 0 : 1;

	if (legacy)
		drive->special_cmd = (ATA_SPECIAL_GEOMETRY | ATA_SPECIAL_RECALIBRATE);
	else
		drive->special_cmd = 0;
	if (OK_TO_RESET_CONTROLLER)
		drive->mult_count = 0;
	if (drive->mult_req != drive->mult_count)
		drive->special_cmd |= ATA_SPECIAL_MMODE;
}

#ifdef CONFIG_PROC_FS

static int smart_enable(ide_drive_t *drive)
{
	struct ata_taskfile args;

	memset(&args, 0, sizeof(args));
	args.taskfile.feature = SMART_ENABLE;
	args.taskfile.low_cylinder = SMART_LCYL_PASS;
	args.taskfile.high_cylinder = SMART_HCYL_PASS;
	args.taskfile.command = WIN_SMART;
	ide_cmd_type_parser(&args);

	return ide_raw_taskfile(drive, &args, NULL);
}

static int get_smart_values(ide_drive_t *drive, u8 *buf)
{
	struct ata_taskfile args;

	memset(&args, 0, sizeof(args));
	args.taskfile.feature = SMART_READ_VALUES;
	args.taskfile.sector_count = 0x01;
	args.taskfile.low_cylinder = SMART_LCYL_PASS;
	args.taskfile.high_cylinder = SMART_HCYL_PASS;
	args.taskfile.command = WIN_SMART;
	ide_cmd_type_parser(&args);

	smart_enable(drive);

	return ide_raw_taskfile(drive, &args, buf);
}

static int get_smart_thresholds(ide_drive_t *drive, u8 *buf)
{
	struct ata_taskfile args;

	memset(&args, 0, sizeof(args));
	args.taskfile.feature = SMART_READ_THRESHOLDS;
	args.taskfile.sector_count = 0x01;
	args.taskfile.low_cylinder = SMART_LCYL_PASS;
	args.taskfile.high_cylinder = SMART_HCYL_PASS;
	args.taskfile.command = WIN_SMART;
	ide_cmd_type_parser(&args);

	smart_enable(drive);

	return ide_raw_taskfile(drive, &args, buf);
}

static int proc_idedisk_read_cache
	(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	ide_drive_t	*drive = (ide_drive_t *) data;
	char		*out = page;
	int		len;

	if (drive->id)
		len = sprintf(out,"%i\n", drive->id->buf_size / 2);
	else
		len = sprintf(out,"(none)\n");
	PROC_IDE_READ_RETURN(page,start,off,count,eof,len);
}

static int proc_idedisk_read_smart_thresholds
	(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	ide_drive_t	*drive = (ide_drive_t *)data;
	int		len = 0, i = 0;

	if (!get_smart_thresholds(drive, page)) {
		unsigned short *val = (unsigned short *) page;
		char *out = ((char *)val) + (SECTOR_WORDS * 4);
		page = out;
		do {
			out += sprintf(out, "%04x%c", le16_to_cpu(*val), (++i & 7) ? ' ' : '\n');
			val += 1;
		} while (i < (SECTOR_WORDS * 2));
		len = out - page;
	}
	PROC_IDE_READ_RETURN(page,start,off,count,eof,len);
}

static int proc_idedisk_read_smart_values
	(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	ide_drive_t	*drive = (ide_drive_t *)data;
	int		len = 0, i = 0;

	if (!get_smart_values(drive, page)) {
		unsigned short *val = (unsigned short *) page;
		char *out = ((char *)val) + (SECTOR_WORDS * 4);
		page = out;
		do {
			out += sprintf(out, "%04x%c", le16_to_cpu(*val), (++i & 7) ? ' ' : '\n');
			val += 1;
		} while (i < (SECTOR_WORDS * 2));
		len = out - page;
	}
	PROC_IDE_READ_RETURN(page,start,off,count,eof,len);
}

static ide_proc_entry_t idedisk_proc[] = {
	{ "cache",		S_IFREG|S_IRUGO,	proc_idedisk_read_cache,		NULL },
	{ "geometry",		S_IFREG|S_IRUGO,	proc_ide_read_geometry,			NULL },
	{ "smart_values",	S_IFREG|S_IRUSR,	proc_idedisk_read_smart_values,		NULL },
	{ "smart_thresholds",	S_IFREG|S_IRUSR,	proc_idedisk_read_smart_thresholds,	NULL },
	{ NULL, 0, NULL, NULL }
};

#else

#define	idedisk_proc	NULL

#endif	/* CONFIG_PROC_FS */

/*
 * This is tightly woven into the driver->special can not touch.
 * DON'T do it again until a total personality rewrite is committed.
 */
static int set_multcount(ide_drive_t *drive, int arg)
{
	struct request rq;

	if (drive->special_cmd & ATA_SPECIAL_MMODE)
		return -EBUSY;

	ide_init_drive_cmd(&rq);

	drive->mult_req = arg;
	drive->special_cmd |= ATA_SPECIAL_MMODE;

	ide_do_drive_cmd (drive, &rq, ide_wait);

	return (drive->mult_count == arg) ? 0 : -EIO;
}

static int set_nowerr(ide_drive_t *drive, int arg)
{
	if (ide_spin_wait_hwgroup(drive))
		return -EBUSY;
	drive->nowerr = arg;
	drive->bad_wstat = arg ? BAD_R_STAT : BAD_W_STAT;
	spin_unlock_irq(&ide_lock);

	return 0;
}

static int write_cache(ide_drive_t *drive, int arg)
{
	struct ata_taskfile args;

	if (!(drive->id->cfs_enable_2 & 0x3000))
		return 1;

	memset(&args, 0, sizeof(args));
	args.taskfile.feature	= (arg) ? SETFEATURES_EN_WCACHE : SETFEATURES_DIS_WCACHE;
	args.taskfile.command	= WIN_SETFEATURES;
	ide_cmd_type_parser(&args);
	ide_raw_taskfile(drive, &args, NULL);

	drive->wcache = arg;

	return 0;
}

static int idedisk_standby(ide_drive_t *drive)
{
	struct ata_taskfile args;

	memset(&args, 0, sizeof(args));
	args.taskfile.command = WIN_STANDBYNOW1;
	ide_cmd_type_parser(&args);

	return ide_raw_taskfile(drive, &args, NULL);
}

static int set_acoustic(ide_drive_t *drive, int arg)
{
	struct ata_taskfile args;

	memset(&args, 0, sizeof(args));
	args.taskfile.feature = (arg)?SETFEATURES_EN_AAM:SETFEATURES_DIS_AAM;
	args.taskfile.sector_count = arg;
	args.taskfile.command = WIN_SETFEATURES;
	ide_cmd_type_parser(&args);
	ide_raw_taskfile(drive, &args, NULL);

	drive->acoustic = arg;

	return 0;
}

static int probe_lba_addressing (ide_drive_t *drive, int arg)
{
	drive->addressing =  0;

	if (!(drive->id->cfs_enable_2 & 0x0400))
                return -EIO;

	drive->addressing = arg;
	return 0;
}

static int set_lba_addressing (ide_drive_t *drive, int arg)
{
	return (probe_lba_addressing(drive, arg));
}

static void idedisk_add_settings(ide_drive_t *drive)
{
	struct hd_driveid *id = drive->id;

	ide_add_setting(drive,	"bios_cyl",		SETTING_RW,					-1,			-1,			TYPE_INT,	0,	65535,				1,	1,	&drive->bios_cyl,		NULL);
	ide_add_setting(drive,	"bios_head",		SETTING_RW,					-1,			-1,			TYPE_BYTE,	0,	255,				1,	1,	&drive->bios_head,		NULL);
	ide_add_setting(drive,	"bios_sect",		SETTING_RW,					-1,			-1,			TYPE_BYTE,	0,	63,				1,	1,	&drive->bios_sect,		NULL);
	ide_add_setting(drive,	"address",		SETTING_RW,					HDIO_GET_ADDRESS,	HDIO_SET_ADDRESS,	TYPE_INTA,	0,	2,				1,	1,	&drive->addressing,	set_lba_addressing);
	ide_add_setting(drive,	"multcount",		id ? SETTING_RW : SETTING_READ,			HDIO_GET_MULTCOUNT,	HDIO_SET_MULTCOUNT,	TYPE_BYTE,	0,	id ? id->max_multsect : 0,	1,	1,	&drive->mult_count,		set_multcount);
	ide_add_setting(drive,	"nowerr",		SETTING_RW,					HDIO_GET_NOWERR,	HDIO_SET_NOWERR,	TYPE_BYTE,	0,	1,				1,	1,	&drive->nowerr,			set_nowerr);
	ide_add_setting(drive,	"lun",			SETTING_RW,					-1,			-1,			TYPE_INT,	0,	7,				1,	1,	&drive->lun,			NULL);
	ide_add_setting(drive,	"wcache",		SETTING_RW,					HDIO_GET_WCACHE,	HDIO_SET_WCACHE,	TYPE_BYTE,	0,	1,				1,	1,	&drive->wcache,			write_cache);
	ide_add_setting(drive,	"acoustic",		SETTING_RW,					HDIO_GET_ACOUSTIC,	HDIO_SET_ACOUSTIC,	TYPE_BYTE,	0,	254,				1,	1,	&drive->acoustic,		set_acoustic);
	ide_add_setting(drive,	"failures",		SETTING_RW,					-1,			-1,			TYPE_INT,	0,	65535,				1,	1,	&drive->failures,		NULL);
	ide_add_setting(drive,	"max_failures",		SETTING_RW,					-1,			-1,			TYPE_INT,	0,	65535,				1,	1,	&drive->max_failures,		NULL);
}

static int idedisk_suspend(struct device *dev, u32 state, u32 level)
{
	ide_drive_t *drive = dev->driver_data;

	/* I hope that every freeze operations from the upper levels have
	 * already been done...
	 */

	if (level != SUSPEND_SAVE_STATE)
		return 0;

	/* wait until all commands are finished */
	printk("ide_disk_suspend()\n");
	while (HWGROUP(drive)->handler)
		yield();

	/* set the drive to standby */
	printk(KERN_INFO "suspending: %s ", drive->name);
	if (ata_ops(drive)) {
		if (ata_ops(drive)->standby)
			ata_ops(drive)->standby(drive);
	}
	drive->blocked = 1;

	return 0;
}

static int idedisk_resume(struct device *dev, u32 level)
{
	ide_drive_t *drive = dev->driver_data;

	if (level != RESUME_RESTORE_STATE)
		return 0;
	if (!drive->blocked)
		panic("ide: Resume but not suspended?\n");

	drive->blocked = 0;
	return 0;
}


/* This is just a hook for the overall driver tree.
 *
 * FIXME: This is soon goig to replace the custom linked list games played up
 * to great extend between the different components of the IDE drivers.
 */

static struct device_driver idedisk_devdrv = {
	suspend: idedisk_suspend,
	resume: idedisk_resume,
};

/*
 * Queries for true maximum capacity of the drive.
 * Returns maximum LBA address (> 0) of the drive, 0 if failed.
 */
static unsigned long native_max_address(struct ata_device *drive)
{
	struct ata_taskfile args;
	unsigned long addr = 0;

	if (!(drive->id->command_set_1 & 0x0400) &&
	    !(drive->id->cfs_enable_2 & 0x0100))
		return addr;

	/* Create IDE/ATA command request structure */
	memset(&args, 0, sizeof(args));
	args.taskfile.device_head = 0x40;
	args.taskfile.command = WIN_READ_NATIVE_MAX;
	args.handler = task_no_data_intr;

	/* submit command request */
	ide_raw_taskfile(drive, &args, NULL);

	/* if OK, compute maximum address value */
	if ((args.taskfile.command & 0x01) == 0) {
		addr = ((args.taskfile.device_head & 0x0f) << 24)
		     | (args.taskfile.high_cylinder << 16)
		     | (args.taskfile.low_cylinder <<  8)
		     | args.taskfile.sector_number;
	}

	addr++;	/* since the return value is (maxlba - 1), we add 1 */

	return addr;
}

static u64 native_max_address_ext(struct ata_device *drive)
{
	struct ata_taskfile args;
	u64 addr = 0;

	/* Create IDE/ATA command request structure */
	memset(&args, 0, sizeof(args));

	args.taskfile.device_head = 0x40;
	args.taskfile.command = WIN_READ_NATIVE_MAX_EXT;
	args.handler = task_no_data_intr;

        /* submit command request */
        ide_raw_taskfile(drive, &args, NULL);

	/* if OK, compute maximum address value */
	if ((args.taskfile.command & 0x01) == 0) {
		u32 high = (args.hobfile.high_cylinder << 16) |
			   (args.hobfile.low_cylinder << 8) |
			    args.hobfile.sector_number;
		u32 low  = (args.taskfile.high_cylinder << 16) |
			   (args.taskfile.low_cylinder << 8) |
			    args.taskfile.sector_number;
		addr = ((u64)high << 24) | low;
	}

	addr++;	/* since the return value is (maxlba - 1), we add 1 */

	return addr;
}

#ifdef CONFIG_IDEDISK_STROKE
/*
 * Sets maximum virtual LBA address of the drive.
 * Returns new maximum virtual LBA address (> 0) or 0 on failure.
 */
static sector_t set_max_address(ide_drive_t *drive, sector_t addr_req)
{
	struct ata_taskfile args;
	sector_t addr_set = 0;

	addr_req--;
	/* Create IDE/ATA command request structure */
	memset(&args, 0, sizeof(args));

	args.taskfile.sector_number = (addr_req >> 0);
	args.taskfile.low_cylinder = (addr_req >> 8);
	args.taskfile.high_cylinder = (addr_req >> 16);

	args.taskfile.device_head = ((addr_req >> 24) & 0x0f) | 0x40;
	args.taskfile.command = WIN_SET_MAX;
	args.handler = task_no_data_intr;
	/* submit command request */
	ide_raw_taskfile(drive, &args, NULL);
	/* if OK, read new maximum address value */
	if ((args.taskfile.command & 0x01) == 0) {
		addr_set = ((args.taskfile.device_head & 0x0f) << 24)
			 | (args.taskfile.high_cylinder << 16)
			 | (args.taskfile.low_cylinder <<  8)
			 | args.taskfile.sector_number;
	}
	addr_set++;
	return addr_set;
}

static u64 set_max_address_ext(ide_drive_t *drive, u64 addr_req)
{
	struct ata_taskfile args;
	u64 addr_set = 0;

	addr_req--;
	/* Create IDE/ATA command request structure */
	memset(&args, 0, sizeof(args));

	args.taskfile.sector_number = (addr_req >>  0);
	args.taskfile.low_cylinder = (addr_req >>= 8);
	args.taskfile.high_cylinder = (addr_req >>= 8);
	args.taskfile.device_head = 0x40;
	args.taskfile.command = WIN_SET_MAX_EXT;

	args.hobfile.sector_number = (addr_req >>= 8);
	args.hobfile.low_cylinder = (addr_req >>= 8);
	args.hobfile.high_cylinder = (addr_req >>= 8);

	args.hobfile.device_head = 0x40;
	args.hobfile.control = (drive->ctl | 0x80);

        args.handler = task_no_data_intr;
	/* submit command request */
	ide_raw_taskfile(drive, &args, NULL);
	/* if OK, compute maximum address value */
	if ((args.taskfile.command & 0x01) == 0) {
		u32 high = (args.hobfile.high_cylinder << 16) |
			   (args.hobfile.low_cylinder << 8) |
			    args.hobfile.sector_number;
		u32 low  = (args.taskfile.high_cylinder << 16) |
			   (args.taskfile.low_cylinder << 8) |
			    args.taskfile.sector_number;
		addr_set = ((u64)high << 24) | low;
	}
	return addr_set;
}

#endif

static void idedisk_setup(struct ata_device *drive)
{
	int i;

	struct hd_driveid *id = drive->id;
	sector_t capacity;
	sector_t set_max;
	int drvid = -1;

	idedisk_add_settings(drive);

	if (id == NULL)
		return;

	/*
	 * CompactFlash cards and their brethern look just like hard drives
	 * to us, but they are removable and don't have a doorlock mechanism.
	 */
	if (drive->removable && !drive_is_flashcard(drive)) {
		/*
		 * Removable disks (eg. SYQUEST); ignore 'WD' drives.
		 */
		if (id->model[0] != 'W' || id->model[1] != 'D') {
			drive->doorlocking = 1;
		}
	}
	for (i = 0; i < MAX_DRIVES; ++i) {
		struct ata_channel *hwif = drive->channel;

		if (drive != &hwif->drives[i])
		    continue;
		drvid = i;
		hwif->gd->de_arr[i] = drive->de;
		if (drive->removable)
			hwif->gd->flags[i] |= GENHD_FL_REMOVABLE;
		break;
	}

	/* Register us within the device tree.
	 */

	if (drvid != -1) {
		sprintf(drive->device.bus_id, "%d", drvid);
		sprintf(drive->device.name, "ide-disk");
		drive->device.driver = &idedisk_devdrv;
		drive->device.parent = &drive->channel->dev;
		drive->device.driver_data = drive;
		device_register(&drive->device);
	}

	/* Extract geometry if we did not already have one for the drive */
	if (!drive->cyl || !drive->head || !drive->sect) {
		drive->cyl     = drive->bios_cyl  = id->cyls;
		drive->head    = drive->bios_head = id->heads;
		drive->sect    = drive->bios_sect = id->sectors;
	}

	/* Handle logical geometry translation by the drive. */
	if ((id->field_valid & 1) && id->cur_cyls &&
	    id->cur_heads && (id->cur_heads <= 16) && id->cur_sectors) {
		drive->cyl  = id->cur_cyls;
		drive->head = id->cur_heads;
		drive->sect = id->cur_sectors;
	}

	/* Use physical geometry if what we have still makes no sense. */
	if (drive->head > 16 && id->heads && id->heads <= 16) {
		drive->cyl  = id->cyls;
		drive->head = id->heads;
		drive->sect = id->sectors;
	}

	/* Calculate drive capacity, and select LBA if possible.
	 * drive->id != NULL is spected
	 *
	 * To compute capacity, this uses either of
	 *
	 *    1. CHS value set by user       (whatever user sets will be trusted)
	 *    2. LBA value from target drive (require new ATA feature)
	 *    3. LBA value from system BIOS  (new one is OK, old one may break)
	 *    4. CHS value from system BIOS  (traditional style)
	 *
	 * in above order (i.e., if value of higher priority is available,
	 * reset will be ignored).
	 */
	capacity = drive->cyl * drive->head * drive->sect;
	set_max = native_max_address(drive);

	drive->capacity = 0;
	drive->select.b.lba = 0;

	if (id->cfs_enable_2 & 0x0400) {
		u64 set_max_ext;
		u64 capacity_2;
		capacity_2 = capacity;
		capacity_2 = id->lba_capacity_2;

		drive->cyl = (unsigned int) capacity_2 / (drive->head * drive->sect);
		drive->head = drive->bios_head = 255;
		drive->sect = drive->bios_sect = 63;

		drive->select.b.lba = 1;
		set_max_ext = native_max_address_ext(drive);
		if (set_max_ext > capacity_2) {
#ifdef CONFIG_IDEDISK_STROKE
			set_max_ext = native_max_address_ext(drive);
			set_max_ext = set_max_address_ext(drive, set_max_ext);
			if (set_max_ext) {
				drive->capacity = capacity_2 = set_max_ext;
				drive->cyl = (unsigned int) set_max_ext / (drive->head * drive->sect);
				drive->select.b.lba = 1;
				drive->id->lba_capacity_2 = capacity_2;
                        }
#else
			printk("%s: setmax_ext LBA %llu, native  %llu\n",
				drive->name, set_max_ext, capacity_2);
#endif
		}
		drive->bios_cyl	= drive->cyl;
		drive->capacity	= capacity_2;
	} else {

		/*
		 * Determine capacity, and use LBA if the drive properly
		 * supports it.
		 */

		if ((id->capability & 2) && lba_capacity_is_ok(id)) {
			capacity = id->lba_capacity;
			drive->cyl = capacity / (drive->head * drive->sect);
			drive->select.b.lba = 1;
		}

		if (set_max > capacity) {
#ifdef CONFIG_IDEDISK_STROKE
			set_max = native_max_address(drive);
			set_max = set_max_address(drive, set_max);
			if (set_max) {
				drive->capacity = capacity = set_max;
				drive->cyl = set_max / (drive->head * drive->sect);
				drive->select.b.lba = 1;
				drive->id->lba_capacity = capacity;
			}
#else
			printk("%s: setmax LBA %lu, native  %lu\n",
					drive->name, set_max, capacity);
#endif
		}

		drive->capacity = capacity;

		if ((id->command_set_2 & 0x0400) && (id->cfs_enable_2 & 0x0400)) {
			drive->capacity = id->lba_capacity_2;
			drive->head = 255;
			drive->sect = 63;
			drive->cyl = (unsigned long)(drive->capacity) / (drive->head * drive->sect);
		}
	}

	/*
	 * If possible, give fdisk access to more of the drive,
	 * by correcting bios_cyls:
	 */
	capacity = idedisk_capacity(drive);
	if ((capacity >= (drive->bios_cyl * drive->bios_sect * drive->bios_head)) &&
	    (!drive->forced_geom) && drive->bios_sect && drive->bios_head)
		drive->bios_cyl = (capacity / drive->bios_sect) / drive->bios_head;
	printk(KERN_INFO "%s: %ld sectors", drive->name, capacity);

#if 0

	/* Right now we avoid this calculation, since it can result in the
	 * usage of not supported compiler internal functions on 32 bit hosts.
	 * However since the calculation appears to be an interesting piece of
	 * number theory let's preserve the formula here.
	 */

	/* Give size in megabytes (MB), not mebibytes (MiB).
	 * We compute the exact rounded value, avoiding overflow.
	 */
	printk(" (%ld MB)", (capacity - capacity/625 + 974)/1950);
#endif

	/* Only print cache size when it was specified.
	 */
	if (id->buf_size)
		printk (" w/%dKiB Cache", id->buf_size/2);

	printk(", CHS=%d/%d/%d",
	       drive->bios_cyl, drive->bios_head, drive->bios_sect);
#ifdef CONFIG_BLK_DEV_IDEDMA
	if (drive->using_dma)
		(void) drive->channel->udma(ide_dma_verbose, drive, NULL);
#endif
	printk("\n");

	drive->mult_count = 0;
	if (id->max_multsect) {
#ifdef CONFIG_IDEDISK_MULTI_MODE
		id->multsect = ((id->max_multsect/2) > 1) ? id->max_multsect : 0;
		id->multsect_valid = id->multsect ? 1 : 0;
		drive->mult_req = id->multsect_valid ? id->max_multsect : INITIAL_MULT_COUNT;
		if (drive->mult_req)
			drive->special_cmd |= ATA_SPECIAL_MMODE;
#else
		/* original, pre IDE-NFG, per request of AC */
		drive->mult_req = INITIAL_MULT_COUNT;
		if (drive->mult_req > id->max_multsect)
			drive->mult_req = id->max_multsect;
		if (drive->mult_req || ((id->multsect_valid & 1) && id->multsect))
			drive->special_cmd |= ATA_SPECIAL_MMODE;
#endif
	}

	/* FIXME: Nowadays there are many chipsets out there which *require* 32
	 * bit IO. Those will most propably not work properly with drives not
	 * supporting this. But right now we don't do anything about this. We
	 * dont' even *warn* the user!
	 */

	drive->channel->no_io_32bit = id->dword_io ? 1 : 0;

	if (drive->id->cfs_enable_2 & 0x3000)
		write_cache(drive, (id->cfs_enable_2 & 0x3000));

	probe_lba_addressing(drive, 1);
}

static int idedisk_cleanup(ide_drive_t *drive)
{
	if (!drive)
	    return 0;

	put_device(&drive->device);
	if ((drive->id->cfs_enable_2 & 0x3000) && drive->wcache)
		if (idedisk_flushcache(drive))
			printk (KERN_INFO "%s: Write Cache FAILED Flushing!\n",
				drive->name);
	return ide_unregister_subdriver(drive);
}

/*
 *      IDE subdriver functions, registered with ide.c
 */
static struct ata_operations idedisk_driver = {
	owner:			THIS_MODULE,
	cleanup:		idedisk_cleanup,
	standby:		idedisk_standby,
	do_request:		idedisk_do_request,
	end_request:		NULL,
	ioctl:			NULL,
	open:			idedisk_open,
	release:		idedisk_release,
	check_media_change:	idedisk_check_media_change,
	revalidate:		NULL, /* use default method */
	pre_reset:		idedisk_pre_reset,
	capacity:		idedisk_capacity,
	special:		idedisk_special,
	proc:			idedisk_proc
};

MODULE_DESCRIPTION("ATA DISK Driver");

static void __exit idedisk_exit (void)
{
	ide_drive_t *drive;
	int failed = 0;

	while ((drive = ide_scan_devices(ATA_DISK, "ide-disk", &idedisk_driver, failed)) != NULL) {
		if (idedisk_cleanup (drive)) {
			printk (KERN_ERR "%s: cleanup_module() called while still busy\n", drive->name);
			failed++;
		}
		/* We must remove proc entries defined in this module.
		   Otherwise we oops while accessing these entries */
#ifdef CONFIG_PROC_FS
		if (drive->proc)
			ide_remove_proc_entries(drive->proc, idedisk_proc);
#endif
	}
}

int idedisk_init (void)
{
	ide_drive_t *drive;
	int failed = 0;

	MOD_INC_USE_COUNT;
	while ((drive = ide_scan_devices(ATA_DISK, "ide-disk", NULL, failed++)) != NULL) {
		if (ide_register_subdriver (drive, &idedisk_driver)) {
			printk (KERN_ERR "ide-disk: %s: Failed to register the driver with ide.c\n", drive->name);
			continue;
		}
		idedisk_setup(drive);
		if ((!drive->head || drive->head > 16) && !drive->select.b.lba) {
			printk(KERN_ERR "%s: INVALID GEOMETRY: %d PHYSICAL HEADS?\n", drive->name, drive->head);
			idedisk_cleanup(drive);
			continue;
		}
		failed--;
	}
	revalidate_drives();
	MOD_DEC_USE_COUNT;
	return 0;
}

module_init(idedisk_init);
module_exit(idedisk_exit);
MODULE_LICENSE("GPL");
