/*
 *  Copyright (C) 1994-1998  Linus Torvalds & authors (see below)
 *
 *  Mostly written by Mark Lord <mlord@pobox.com>
 *                and Gadi Oxman <gadio@netvision.net.il>
 *                and Andre Hedrick <andre@linux-ide.org>
 *
 *  See linux/MAINTAINERS for address of current maintainer.
 *
 * This is the IDE probe module, as evolved from hd.c and ide.c.
 *
 * Version 1.00		move drive probing code from ide.c to ide-probe.c
 * Version 1.01		fix compilation problem for m68k
 * Version 1.02		increase WAIT_PIDENTIFY to avoid CD-ROM locking at boot
 *			 by Andrea Arcangeli
 * Version 1.03		fix for (hwif->chipset == ide_4drives)
 * Version 1.04		fixed buggy treatments of known flash memory cards
 *
 * Version 1.05		fix for (hwif->chipset == ide_pdc4030)
 *			added ide6/7/8/9
 *			allowed for secondary flash card to be detectable
 *			 with new flag : drive->ata_flash : 1;
 * Version 1.06		stream line request queue and prep for cascade project.
 * Version 1.07		max_sect <= 255; slower disks would get behind and
 *			then fall over when they get to 256.	Paul G.
 */

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
#include <linux/spinlock.h>
#include <linux/pci.h>

#include <asm/byteorder.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/io.h>

static inline void do_identify (ide_drive_t *drive, byte cmd)
{
	int bswap = 1;
	struct hd_driveid *id;

	id = drive->id = kmalloc (SECTOR_WORDS*4, GFP_ATOMIC);	/* called with interrupts disabled! */
	if (!id) {
		printk(KERN_WARNING "(ide-probe::do_identify) Out of memory.\n");
		goto err_kmalloc;
	}

	/* Read 512 bytes of id info.
	 *
	 * Please note that it is well known that some *very* old drives are
	 * able to provide only 256 of them, since this was the amount read by
	 * DOS.
	 *
	 * However let's try to get away with this...
	 */

#if 1
	ata_read(drive, id, SECTOR_WORDS);
#else
        {
                unsigned long   *ptr = (unsigned long *)id ;
                unsigned long   lcount = 256/2 ;
                // printk("IDE_DATA_REG = %#lx",IDE_DATA_REG);
                while( lcount-- )
                        *ptr++ = inl(IDE_DATA_REG);
        }
#endif
	ide__sti();	/* local CPU only */
	ide_fix_driveid(id);

	if (id->word156 == 0x4d42) {
		printk("%s: drive->id->word156 == 0x%04x \n", drive->name, drive->id->word156);
	}

	if (!drive->forced_lun)
		drive->last_lun = id->last_lun & 0x7;
#if defined (CONFIG_SCSI_EATA_DMA) || defined (CONFIG_SCSI_EATA_PIO) || defined (CONFIG_SCSI_EATA)
	/*
	 * EATA SCSI controllers do a hardware ATA emulation:
	 * Ignore them if there is a driver for them available.
	 */
	if ((id->model[0] == 'P' && id->model[1] == 'M')
	 || (id->model[0] == 'S' && id->model[1] == 'K')) {
		printk("%s: EATA SCSI HBA %.10s\n", drive->name, id->model);
		goto err_misc;
	}
#endif

	/*
	 *  WIN_IDENTIFY returns little-endian info,
	 *  WIN_PIDENTIFY *usually* returns little-endian info.
	 */
	if (cmd == WIN_PIDENTIFY) {
		if ((id->model[0] == 'N' && id->model[1] == 'E') /* NEC */
		 || (id->model[0] == 'F' && id->model[1] == 'X') /* Mitsumi */
		 || (id->model[0] == 'P' && id->model[1] == 'i'))/* Pioneer */
			bswap ^= 1;	/* Vertos drives may still be weird */
	}
	ide_fixstring (id->model,     sizeof(id->model),     bswap);
	ide_fixstring (id->fw_rev,    sizeof(id->fw_rev),    bswap);
	ide_fixstring (id->serial_no, sizeof(id->serial_no), bswap);

	if (strstr(id->model, "E X A B Y T E N E S T"))
		goto err_misc;

	id->model[sizeof(id->model)-1] = '\0';	/* we depend on this a lot! */
	printk("%s: %s, ", drive->name, id->model);
	drive->present = 1;

	/*
	 * Check for an ATAPI device
	 */
	if (cmd == WIN_PIDENTIFY) {
		byte type = (id->config >> 8) & 0x1f;
		printk("ATAPI ");
#ifdef CONFIG_BLK_DEV_PDC4030
		if (drive->channel->unit == 1 && drive->channel->chipset == ide_pdc4030) {
			printk(" -- not supported on 2nd Promise port\n");
			goto err_misc;
		}
#endif
		switch (type) {
			case ATA_FLOPPY:
				if (!strstr(id->model, "CD-ROM")) {
					if (!strstr(id->model, "oppy") && !strstr(id->model, "poyp") && !strstr(id->model, "ZIP"))
						printk("cdrom or floppy?, assuming ");
					if (drive->type != ATA_ROM) {
						printk ("FLOPPY");
						break;
					}
				}
				type = ATA_ROM;	/* Early cdrom models used zero */
			case ATA_ROM:
				drive->removable = 1;
#ifdef CONFIG_PPC
				/* kludge for Apple PowerBook internal zip */
				if (!strstr(id->model, "CD-ROM") && strstr(id->model, "ZIP")) {
					printk ("FLOPPY");
					type = ATA_FLOPPY;
					break;
				}
#endif
				printk ("CD/DVD-ROM");
				break;
			case ATA_TAPE:
				printk ("TAPE");
				break;
			case ATA_MOD:
				printk ("OPTICAL");
				drive->removable = 1;
				break;
			default:
				printk("UNKNOWN (type %d)", type);
				break;
		}
		printk (" drive\n");
		drive->type = type;
		return;
	}

	/*
	 * Not an ATAPI device: looks like a "regular" hard disk
	 */
	if (id->config & (1<<7))
		drive->removable = 1;

	/*
	 * FIXME: This is just plain ugly or plain unnecessary.
	 *
	 * Prevent long system lockup probing later for non-existant slave
	 * drive if the hwif is actually a flash memory card of some variety:
	 */

	if (drive_is_flashcard(drive)) {
		ide_drive_t *mate = &drive->channel->drives[1 ^ drive->select.b.unit];
		if (!mate->ata_flash) {
			mate->present = 0;
			mate->noprobe = 1;
		}
	}
	drive->type = ATA_DISK;
	printk("ATA DISK drive\n");

	/* Initialize our quirk list. */
	if (drive->channel->quirkproc)
		drive->quirk_list = drive->channel->quirkproc(drive);

	return;

err_misc:
	kfree(id);
err_kmalloc:
	drive->present = 0;
	return;
}

/*
 * try_to_identify() sends an ATA(PI) IDENTIFY request to a drive
 * and waits for a response.  It also monitors irqs while this is
 * happening, in hope of automatically determining which one is
 * being used by the interface.
 *
 * Returns:	0  device was identified
 *		1  device timed-out (no response to identify request)
 *		2  device aborted the command (refused to identify itself)
 */
static int actual_try_to_identify (ide_drive_t *drive, byte cmd)
{
	int rc;
	ide_ioreg_t hd_status;
	unsigned long timeout;
	byte s, a;

	if (IDE_CONTROL_REG) {
		/* take a deep breath */
		ide_delay_50ms();
		a = IN_BYTE(IDE_ALTSTATUS_REG);
		s = IN_BYTE(IDE_STATUS_REG);
		if ((a ^ s) & ~INDEX_STAT) {
			printk("%s: probing with STATUS(0x%02x) instead of ALTSTATUS(0x%02x)\n", drive->name, s, a);
			hd_status = IDE_STATUS_REG;	/* ancient Seagate drives, broken interfaces */
		} else {
			hd_status = IDE_ALTSTATUS_REG;	/* use non-intrusive polling */
		}
	} else {
		ide_delay_50ms();
		hd_status = IDE_STATUS_REG;
	}

	/* set features register for atapi identify command to be sure of reply */
	if ((cmd == WIN_PIDENTIFY))
		OUT_BYTE(0,IDE_FEATURE_REG);	/* disable dma & overlap */

#if CONFIG_BLK_DEV_PDC4030
	if (drive->channel->chipset == ide_pdc4030) {
		/* DC4030 hosted drives need their own identify... */
		extern int pdc4030_identify(ide_drive_t *);
		if (pdc4030_identify(drive)) {
			return 1;
		}
	} else
#endif /* CONFIG_BLK_DEV_PDC4030 */
		OUT_BYTE(cmd,IDE_COMMAND_REG);		/* ask drive for ID */
	timeout = ((cmd == WIN_IDENTIFY) ? WAIT_WORSTCASE : WAIT_PIDENTIFY) / 2;
	timeout += jiffies;
	do {
		if (0 < (signed long)(jiffies - timeout)) {
			return 1;	/* drive timed-out */
		}
		ide_delay_50ms();		/* give drive a breather */
	} while (IN_BYTE(hd_status) & BUSY_STAT);

	ide_delay_50ms();		/* wait for IRQ and DRQ_STAT */
	if (OK_STAT(GET_STAT(),DRQ_STAT,BAD_R_STAT)) {
		unsigned long flags;
		__save_flags(flags);	/* local CPU only */
		__cli();		/* local CPU only; some systems need this */
		do_identify(drive, cmd); /* drive returned ID */
		rc = 0;			/* drive responded with ID */
		(void) GET_STAT();	/* clear drive IRQ */
		__restore_flags(flags);	/* local CPU only */
	} else
		rc = 2;			/* drive refused ID */
	return rc;
}

static int try_to_identify (ide_drive_t *drive, byte cmd)
{
	int retval;
	int autoprobe = 0;
	unsigned long cookie = 0;

	if (IDE_CONTROL_REG && !drive->channel->irq) {
		autoprobe = 1;
		cookie = probe_irq_on();
		OUT_BYTE(drive->ctl,IDE_CONTROL_REG);	/* enable device irq */
	}

	retval = actual_try_to_identify(drive, cmd);

	if (autoprobe) {
		int irq;
		OUT_BYTE(drive->ctl | 0x02, IDE_CONTROL_REG);	/* mask device irq */
		GET_STAT();			/* clear drive IRQ */
		udelay(5);
		irq = probe_irq_off(cookie);
		if (!drive->channel->irq) {
			if (irq > 0)
				drive->channel->irq = irq;
			else	/* Mmmm.. multiple IRQs.. don't know which was ours */
				printk("%s: IRQ probe failed (0x%lx)\n", drive->name, cookie);
		}
	}
	return retval;
}


/*
 * do_probe() has the difficult job of finding a drive if it exists,
 * without getting hung up if it doesn't exist, without trampling on
 * ethernet cards, and without leaving any IRQs dangling to haunt us later.
 *
 * If a drive is "known" to exist (from CMOS or kernel parameters),
 * but does not respond right away, the probe will "hang in there"
 * for the maximum wait time (about 30 seconds), otherwise it will
 * exit much more quickly.
 *
 * Returns:	0  device was identified
 *		1  device timed-out (no response to identify request)
 *		2  device aborted the command (refused to identify itself)
 *		3  bad status from device (possible for ATAPI drives)
 *		4  probe was not attempted because failure was obvious
 */
static int do_probe (ide_drive_t *drive, byte cmd)
{
	int rc;
	struct ata_channel *hwif = drive->channel;
	if (drive->present) {	/* avoid waiting for inappropriate probes */
		if ((drive->type != ATA_DISK) && (cmd == WIN_IDENTIFY))
			return 4;
	}
#ifdef DEBUG
	printk("probing for %s: present=%d, type=%d, probetype=%s\n",
		drive->name, drive->present, drive->type,
		(cmd == WIN_IDENTIFY) ? "ATA" : "ATAPI");
#endif
	ide_delay_50ms();	/* needed for some systems (e.g. crw9624 as drive0 with disk as slave) */
	SELECT_DRIVE(hwif,drive);
	ide_delay_50ms();
	if (IN_BYTE(IDE_SELECT_REG) != drive->select.all && !drive->present) {
		if (drive->select.b.unit != 0) {
			SELECT_DRIVE(hwif,&hwif->drives[0]);	/* exit with drive0 selected */
			ide_delay_50ms();		/* allow BUSY_STAT to assert & clear */
		}
		return 3;    /* no i/f present: mmm.. this should be a 4 -ml */
	}

	if (OK_STAT(GET_STAT(),READY_STAT,BUSY_STAT)
	 || drive->present || cmd == WIN_PIDENTIFY)
	{
		if ((rc = try_to_identify(drive,cmd)))   /* send cmd and wait */
			rc = try_to_identify(drive,cmd); /* failed: try again */
		if (rc == 1 && cmd == WIN_PIDENTIFY && drive->autotune != 2) {
			unsigned long timeout;
			printk("%s: no response (status = 0x%02x), resetting drive\n", drive->name, GET_STAT());
			ide_delay_50ms();
			OUT_BYTE (drive->select.all, IDE_SELECT_REG);
			ide_delay_50ms();
			OUT_BYTE(WIN_SRST, IDE_COMMAND_REG);
			timeout = jiffies;
			while ((GET_STAT() & BUSY_STAT) && time_before(jiffies, timeout + WAIT_WORSTCASE))
				ide_delay_50ms();
			rc = try_to_identify(drive, cmd);
		}
		if (rc == 1)
			printk("%s: no response (status = 0x%02x)\n", drive->name, GET_STAT());
		(void) GET_STAT();		/* ensure drive irq is clear */
	} else {
		rc = 3;				/* not present or maybe ATAPI */
	}
	if (drive->select.b.unit != 0) {
		SELECT_DRIVE(hwif,&hwif->drives[0]);	/* exit with drive0 selected */
		ide_delay_50ms();
		(void) GET_STAT();		/* ensure drive irq is clear */
	}
	return rc;
}

/*
 *
 */
static void enable_nest(ide_drive_t *drive)
{
	unsigned long timeout;

	printk("%s: enabling %s -- ", drive->channel->name, drive->id->model);
	SELECT_DRIVE(drive->channel, drive);
	ide_delay_50ms();
	OUT_BYTE(EXABYTE_ENABLE_NEST, IDE_COMMAND_REG);
	timeout = jiffies + WAIT_WORSTCASE;
	do {
		if (jiffies > timeout) {
			printk("failed (timeout)\n");
			return;
		}
		ide_delay_50ms();
	} while (GET_STAT() & BUSY_STAT);
	ide_delay_50ms();
	if (!OK_STAT(GET_STAT(), 0, BAD_STAT))
		printk("failed (status = 0x%02x)\n", GET_STAT());
	else
		printk("success\n");
	if (do_probe(drive, WIN_IDENTIFY) >= 2) {	/* if !(success||timed-out) */
		(void) do_probe(drive, WIN_PIDENTIFY);	/* look for ATAPI device */
	}
}

/*
 * Tests for existence of a given drive using do_probe().
 */
static inline void probe_for_drive (ide_drive_t *drive)
{
	if (drive->noprobe)			/* skip probing? */
		return;
	if (do_probe(drive, WIN_IDENTIFY) >= 2) { /* if !(success||timed-out) */
		do_probe(drive, WIN_PIDENTIFY); /* look for ATAPI device */
	}
	if (drive->id && strstr(drive->id->model, "E X A B Y T E N E S T"))
		enable_nest(drive);
	if (!drive->present)
		return;			/* drive not found */
	if (drive->id == NULL) {		/* identification failed? */
		if (drive->type == ATA_DISK) {
			printk ("%s: non-IDE drive, CHS=%d/%d/%d\n",
			 drive->name, drive->cyl, drive->head, drive->sect);
		} else if (drive->type == ATA_ROM) {
			printk("%s: ATAPI cdrom (?)\n", drive->name);
		} else {
			drive->present = 0;	/* nuke it */
		}
	}
}

/*
 * This routine only knows how to look for drive units 0 and 1
 * on an interface, so any setting of MAX_DRIVES > 2 won't work here.
 */
static void channel_probe(struct ata_channel *ch)
{
	unsigned int unit;
	unsigned long flags;
	int error;

	if (ch->noprobe)
		return;

	ch->straight8 = 0;

	__save_flags(flags);	/* local CPU only */
	__sti();		/* local CPU only; needed for jiffies and irq probing */

	/*
	 * Check for the presence of a channel by probing for drives on it.
	 */
	for (unit = 0; unit < MAX_DRIVES; ++unit) {
		struct ata_device *drive = &ch->drives[unit];

		probe_for_drive(drive);

		/* drive found, there is a channel it is attached too. */
		if (drive->present)
			ch->present = 1;
	}

	if (!ch->present)
		goto not_found;

	error = 0;

	if (((unsigned long)ch->io_ports[IDE_DATA_OFFSET] | 7) ==
			((unsigned long)ch->io_ports[IDE_STATUS_OFFSET])) {
		error += !request_region(ch->io_ports[IDE_DATA_OFFSET], 8, ch->name);
		ch->straight8 = 1;
	} else {
		if (ch->io_ports[IDE_DATA_OFFSET])
			error += !request_region(ch->io_ports[IDE_DATA_OFFSET], 1, ch->name);
		if (ch->io_ports[IDE_ERROR_OFFSET])
			error += !request_region(ch->io_ports[IDE_ERROR_OFFSET], 1, ch->name);
		if (ch->io_ports[IDE_NSECTOR_OFFSET])
			error += !request_region(ch->io_ports[IDE_NSECTOR_OFFSET], 1, ch->name);
		if (ch->io_ports[IDE_SECTOR_OFFSET])
			error += !request_region(ch->io_ports[IDE_SECTOR_OFFSET], 1, ch->name);
		if (ch->io_ports[IDE_LCYL_OFFSET])
			error += !request_region(ch->io_ports[IDE_LCYL_OFFSET], 1, ch->name);
		if (ch->io_ports[IDE_HCYL_OFFSET])
			error += !request_region(ch->io_ports[IDE_HCYL_OFFSET], 1, ch->name);
		if (ch->io_ports[IDE_SELECT_OFFSET])
			error += !request_region(ch->io_ports[IDE_SELECT_OFFSET], 1, ch->name);
		if (ch->io_ports[IDE_STATUS_OFFSET])
			error += !request_region(ch->io_ports[IDE_STATUS_OFFSET], 1, ch->name);

	}
	if (ch->io_ports[IDE_CONTROL_OFFSET])
		error += !request_region(ch->io_ports[IDE_CONTROL_OFFSET], 1, ch->name);
#if defined(CONFIG_AMIGA) || defined(CONFIG_MAC)
	if (ch->io_ports[IDE_IRQ_OFFSET])
		error += !request_region(ch->io_ports[IDE_IRQ_OFFSET], 1, ch->name);
#endif

	/* Some neccessary register area was already used. Skip this device.
	 */

	if (
#if CONFIG_BLK_DEV_PDC4030
			(ch->chipset != ide_pdc4030 || ch->unit == 0) &&
#endif
			error) {

		/* FIXME: We should be dealing properly with partial IO region
		 * allocations here.
		 */

		ch->present = 0;
		printk("%s: error: ports already in use!\n", ch->name);
	}

	if (!ch->present)
		goto not_found;

	/* Register this hardware interface within the global device tree.
	 */
	sprintf(ch->dev.bus_id, "%04x", ch->io_ports[IDE_DATA_OFFSET]);
	sprintf(ch->dev.name, "ide");
	ch->dev.driver_data = ch;
#ifdef CONFIG_BLK_DEV_IDEPCI
	if (ch->pci_dev)
		ch->dev.parent = &ch->pci_dev->dev;
	else
#endif
		ch->dev.parent = NULL; /* Would like to do = &device_legacy */

	device_register(&ch->dev);

	if (ch->io_ports[IDE_CONTROL_OFFSET] && ch->reset) {
		unsigned long timeout = jiffies + WAIT_WORSTCASE;
		byte stat;

		printk("%s: reset\n", ch->name);
		OUT_BYTE(12, ch->io_ports[IDE_CONTROL_OFFSET]);
		udelay(10);
		OUT_BYTE(8, ch->io_ports[IDE_CONTROL_OFFSET]);
		do {
			ide_delay_50ms();
			stat = IN_BYTE(ch->io_ports[IDE_STATUS_OFFSET]);
		} while ((stat & BUSY_STAT) && 0 < (signed long)(timeout - jiffies));
	}

	__restore_flags(flags);	/* local CPU only */

	/*
	 * Now setup the PIO transfer modes of the drives on this channel.
	 */
	for (unit = 0; unit < MAX_DRIVES; ++unit) {
		struct ata_device *drive = &ch->drives[unit];

		if (drive->present && (drive->autotune == 1)) {
			if (drive->channel->tuneproc)
				drive->channel->tuneproc(drive, 255);	/* auto-tune PIO mode */
		}
	}

	return;

not_found:
	__restore_flags(flags);
}

/*
 * init request queue
 */
static void init_device_queue(struct ata_device *drive)
{
	request_queue_t *q = &drive->queue;
	int max_sectors = 255;

	q->queuedata = drive->channel;
	blk_init_queue(q, do_ide_request, &ide_lock);
	blk_queue_segment_boundary(q, 0xffff);

	/* IDE can do up to 128K per request, pdc4030 needs smaller limit */
#ifdef CONFIG_BLK_DEV_PDC4030
	if (drive->channel->chipset == ide_pdc4030)
		max_sectors = 127;
#endif
	blk_queue_max_sectors(q, max_sectors);

	/* IDE DMA can do PRD_ENTRIES number of segments. */
	blk_queue_max_hw_segments(q, PRD_SEGMENTS);

	/* This is a driver limit and could be eliminated. */
	blk_queue_max_phys_segments(q, PRD_SEGMENTS);
}

#if MAX_HWIFS > 1

/*
 * This is used to simplify logic in init_irq() below.
 *
 * A loophole here is that we may not know about a particular hwif's irq until
 * after that hwif is actually probed/initialized..  This could be a problem
 * for the case where an hwif is on a dual interface that requires
 * serialization (eg. cmd640) and another hwif using one of the same irqs is
 * initialized beforehand.
 *
 * This routine detects and reports such situations, but does not fix them.
 */
static void save_match(struct ata_channel *hwif, struct ata_channel *new,
		struct ata_channel **match)
{
	struct ata_channel *m = *match;

	if (m && m->hwgroup && m->hwgroup != new->hwgroup) {
		if (!new->hwgroup)
			return;
		printk("%s: potential irq problem with %s and %s\n", hwif->name, new->name, m->name);
	}
	if (!m || m->irq != hwif->irq) /* don't undo a prior perfect match */
		*match = new;
}
#endif

/*
 * This routine sets up the irq for an ide interface, and creates a new hwgroup
 * for the irq/channel if none was previously assigned.
 *
 * Much of the code is for correctly detecting/handling irq sharing and irq
 * serialization situations.  This is somewhat complex because it handles
 * static as well as dynamic (PCMCIA) IDE interfaces.
 *
 * The SA_INTERRUPT in sa_flags means ata_irq_request() is always entered with
 * interrupts completely disabled.  This can be bad for interrupt latency, but
 * anything else has led to problems on some machines.  We re-enable interrupts
 * as much as we can safely do in most places.
 */
static int init_irq(struct ata_channel *ch)
{
	unsigned long flags;
	int i;
	ide_hwgroup_t *hwgroup;
	ide_hwgroup_t *new_hwgroup;
	struct ata_channel *match = NULL;


	/* Allocate the buffer and potentially sleep first */

	new_hwgroup = kmalloc(sizeof(ide_hwgroup_t),GFP_KERNEL);

	spin_lock_irqsave(&ide_lock, flags);
	ch->hwgroup = NULL;

#if MAX_HWIFS > 1
	/*
	 * Group up with any other channels that share our irq(s).
	 */
	for (i = 0; i < MAX_HWIFS; ++i) {
		struct ata_channel *h = &ide_hwifs[i];

		if (h->hwgroup) {  /* scan only initialized channels */
			if (ch->irq == h->irq) {
				ch->sharing_irq = h->sharing_irq = 1;
				if (ch->chipset != ide_pci || h->chipset != ide_pci)
					save_match(ch, h, &match);

				/* FIXME: This is still confusing. What would
				 * happen if we match-ed two times?
				 */

				if (ch->serialized || h->serialized)
					save_match(ch, h, &match);
			}
		}
	}
#endif
	/*
	 * If we are still without a hwgroup, then form a new one
	 */
	if (match) {
		hwgroup = match->hwgroup;
		if(new_hwgroup)
			kfree(new_hwgroup);
	} else {
		hwgroup = new_hwgroup;
		if (!hwgroup) {
			spin_unlock_irqrestore(&ide_lock, flags);
			return 1;
		}
		memset(hwgroup, 0, sizeof(*hwgroup));
		init_timer(&hwgroup->timer);
		hwgroup->timer.function = &ide_timer_expiry;
		hwgroup->timer.data = (unsigned long) hwgroup;
	}

	/*
	 * Allocate the irq, if not already obtained for another channel
	 */
	if (!match || match->irq != ch->irq) {
#ifdef CONFIG_IDEPCI_SHARE_IRQ
		int sa = IDE_CHIPSET_IS_PCI(ch->chipset) ? SA_SHIRQ : SA_INTERRUPT;
#else
		int sa = IDE_CHIPSET_IS_PCI(ch->chipset) ? SA_INTERRUPT|SA_SHIRQ : SA_INTERRUPT;
#endif

		if (ch->io_ports[IDE_CONTROL_OFFSET])
			OUT_BYTE(0x08, ch->io_ports[IDE_CONTROL_OFFSET]); /* clear nIEN */

		if (request_irq(ch->irq, &ata_irq_request, sa, ch->name, ch)) {
			if (!match)
				kfree(hwgroup);
			spin_unlock_irqrestore(&ide_lock, flags);

			return 1;
		}
	}

	/*
	 * Everything is okay. Tag us as member of this hardware group.
	 */
	ch->hwgroup = hwgroup;
	for (i = 0; i < MAX_DRIVES; ++i) {
		struct ata_device *drive = &ch->drives[i];

		if (!drive->present)
			continue;

		if (!hwgroup->XXX_drive)
			hwgroup->XXX_drive = drive;

		init_device_queue(drive);
	}
	spin_unlock_irqrestore(&ide_lock, flags);

#if !defined(__mc68000__) && !defined(CONFIG_APUS) && !defined(__sparc__)
	printk("%s at 0x%03x-0x%03x,0x%03x on irq %d", ch->name,
		ch->io_ports[IDE_DATA_OFFSET],
		ch->io_ports[IDE_DATA_OFFSET]+7,
		ch->io_ports[IDE_CONTROL_OFFSET], ch->irq);
#elif defined(__sparc__)
	printk("%s at 0x%03lx-0x%03lx,0x%03lx on irq %s", ch->name,
		ch->io_ports[IDE_DATA_OFFSET],
		ch->io_ports[IDE_DATA_OFFSET]+7,
		ch->io_ports[IDE_CONTROL_OFFSET], __irq_itoa(ch->irq));
#else
	printk("%s at %p on irq 0x%08x", ch->name,
		ch->io_ports[IDE_DATA_OFFSET], ch->irq);
#endif /* __mc68000__ && CONFIG_APUS */
	if (match)
		printk(" (%sed with %s)",
			ch->sharing_irq ? "shar" : "serializ", match->name);
	printk("\n");

	return 0;
}

/*
 * init_gendisk() (as opposed to ide_geninit) is called for each major device,
 * after probing for drives, to allocate partition tables and other data
 * structures needed for the routines in genhd.c.  ide_geninit() gets called
 * somewhat later, during the partition check.
 */
static void init_gendisk(struct ata_channel *hwif)
{
	struct gendisk *gd;
	unsigned int unit, minors, i;
	extern devfs_handle_t ide_devfs_handle;

	minors = MAX_DRIVES * (1 << PARTN_BITS);

	gd = kmalloc (sizeof(struct gendisk), GFP_KERNEL);
	if (!gd)
		goto err_kmalloc_gd;

	gd->sizes = kmalloc (minors * sizeof(int), GFP_KERNEL);
	if (!gd->sizes)
		goto err_kmalloc_gd_sizes;

	gd->part = kmalloc (minors * sizeof(struct hd_struct), GFP_KERNEL);
	if (!gd->part)
		goto err_kmalloc_gd_part;
	memset(gd->part, 0, minors * sizeof(struct hd_struct));

	blksize_size[hwif->major] = kmalloc (minors*sizeof(int), GFP_KERNEL);
	if (!blksize_size[hwif->major])
		goto err_kmalloc_bs;
	for (i = 0; i < minors; ++i)
	    blksize_size[hwif->major][i] = BLOCK_SIZE;

	for (unit = 0; unit < MAX_DRIVES; ++unit)
		hwif->drives[unit].part = &gd->part[unit << PARTN_BITS];

	gd->major	= hwif->major;		/* our major device number */
	gd->major_name	= IDE_MAJOR_NAME;	/* treated special in genhd.c */
	gd->minor_shift	= PARTN_BITS;		/* num bits for partitions */
	gd->nr_real	= MAX_DRIVES;		/* current num real drives */
	gd->next	= NULL;			/* linked list of major devs */
	gd->fops        = ide_fops;             /* file operations */
	gd->de_arr	= kmalloc(sizeof(*gd->de_arr) * MAX_DRIVES, GFP_KERNEL);
	gd->flags	= kmalloc(sizeof(*gd->flags) * MAX_DRIVES, GFP_KERNEL);
	if (gd->de_arr)
		memset(gd->de_arr, 0, sizeof(*gd->de_arr) * MAX_DRIVES);
	if (gd->flags)
		memset(gd->flags, 0, sizeof(*gd->flags) * MAX_DRIVES);

	hwif->gd = gd;
	add_gendisk(gd);

	for (unit = 0; unit < MAX_DRIVES; ++unit) {
		char name[80];

		ide_add_generic_settings(hwif->drives + unit);
		hwif->drives[unit].dn = ((hwif->unit ? 2 : 0) + unit);
		sprintf (name, "host%d/bus%d/target%d/lun%d",
			hwif->index, hwif->unit, unit, hwif->drives[unit].lun);
		if (hwif->drives[unit].present)
			hwif->drives[unit].de = devfs_mk_dir(ide_devfs_handle, name, NULL);
	}
	return;

err_kmalloc_bs:
	kfree(gd->part);
err_kmalloc_gd_part:
	kfree(gd->sizes);
err_kmalloc_gd_sizes:
	kfree(gd);
err_kmalloc_gd:
	printk(KERN_CRIT "(ide::init_gendisk) Out of memory\n");
	return;
}

/*
 * Returns the queue which corresponds to a given device.
 *
 * FIXME: this should take struct block_device * as argument in future.
 */

static request_queue_t *ata_get_queue(kdev_t dev)
{
	struct ata_channel *ch = (struct ata_channel *)blk_dev[major(dev)].data;

	/* FIXME: ALLERT: This discriminates between master and slave! */
	return &ch->drives[DEVICE_NR(dev) & 1].queue;
}

static void channel_init(struct ata_channel *ch)
{
	if (!ch->present)
		return;

	/* we set it back to 1 if all is ok below */
	ch->present = 0;

	if (!ch->irq) {
		if (!(ch->irq = ide_default_irq(ch->io_ports[IDE_DATA_OFFSET]))) {
			printk("%s: DISABLED, NO IRQ\n", ch->name);

			return;
		}
	}
#ifdef CONFIG_BLK_DEV_HD
	if (ch->irq == HD_IRQ && ch->io_ports[IDE_DATA_OFFSET] != HD_DATA) {
		printk("%s: CANNOT SHARE IRQ WITH OLD HARDDISK DRIVER (hd.c)\n", ch->name);

		return;
	}
#endif

	if (devfs_register_blkdev (ch->major, ch->name, ide_fops)) {
		printk("%s: UNABLE TO GET MAJOR NUMBER %d\n", ch->name, ch->major);

		return;
	}

	if (init_irq(ch)) {
		int irq = ch->irq;
		/*
		 * It failed to initialise. Find the default IRQ for
		 * this port and try that.
		 */
		if (!(ch->irq = ide_default_irq(ch->io_ports[IDE_DATA_OFFSET]))) {
			printk(KERN_INFO "%s: disabled; unable to get IRQ %d.\n", ch->name, irq);
			(void) unregister_blkdev (ch->major, ch->name);

			return;
		}
		if (init_irq(ch)) {
			printk(KERN_INFO "%s: probed IRQ %d and default IRQ %d failed.\n",
				ch->name, irq, ch->irq);
			(void) unregister_blkdev(ch->major, ch->name);

			return;
		}
		printk(KERN_INFO "%s: probed IRQ %d failed, using default.\n", ch->name, ch->irq);
	}

	init_gendisk(ch);
	blk_dev[ch->major].data = ch;
	blk_dev[ch->major].queue = ata_get_queue;

	/* all went well, flag this channel entry as valid */
	ch->present = 1;

	return;
}

int ideprobe_init (void)
{
	unsigned int index;
	int probe[MAX_HWIFS];

	memset(probe, 0, MAX_HWIFS * sizeof(int));
	for (index = 0; index < MAX_HWIFS; ++index)
		probe[index] = !ide_hwifs[index].present;

	/*
	 * Probe for drives in the usual way.. CMOS/BIOS, then poke at ports
	 */
	for (index = 0; index < MAX_HWIFS; ++index)
		if (probe[index])
			channel_probe(&ide_hwifs[index]);
	for (index = 0; index < MAX_HWIFS; ++index)
		if (probe[index])
			channel_init(&ide_hwifs[index]);
	return 0;
}
