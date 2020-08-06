/* doshd.c copyright (C) 1994 Yggdrasil Computing, Incorporated
 * 4880 Stevens Creek Blvd. Suite 205
 * San Jose, CA 95129-1034
 * USA
 *
 * Tel (408) 261-6630
 * Fax (408) 261-6631
 *
 * This file is part of the Linux Kernel
 *
 * Linux is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * Linux is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Linux; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linuxmt/types.h>
#include <linuxmt/kernel.h>
#include <linuxmt/sched.h>
#include <linuxmt/errno.h>
#include <linuxmt/genhd.h>
#include <linuxmt/hdreg.h>
#include <linuxmt/biosparm.h>
#include <linuxmt/major.h>
#include <linuxmt/bioshd.h>
#include <linuxmt/fs.h>
#include <linuxmt/string.h>
#include <linuxmt/mm.h>
#include <linuxmt/config.h>
#include <linuxmt/debug.h>

#include <arch/segment.h>
#include <arch/system.h>
#include <arch/irq.h>

#ifdef CONFIG_BLK_DEV_BIOS

/* the following must match with /dev minor numbering scheme*/
#define NUM_MINOR	32	/* max minor devices per drive*/
#define MINOR_SHIFT	5	/* =log2(NUM_MINOR) shift to get drive num*/
#define NUM_DRIVES	8	/* =256/NUM_MINOR max number of drives*/
#define DRIVE_FD0	4	/* first floppy drive =NUM_DRIVES/2*/
#define DRIVE_FD1	5	/* second floppy drive*/

#define MAJOR_NR BIOSHD_MAJOR
#define BIOSDISK

#include "blk.h"

struct elks_disk_parms {
    __u16 track_max;		/* number of tracks, little-endian */
    __u8 sect_max;		/* number of sectors per track */
    __u8 head_max;		/* number of disk sides/heads */
    __u8 size;			/* size of parameter block (everything before
				   this point) */
    __u8 marker[2];		/* should be "eL" */
} __attribute__((packed));

struct elks_boot_sect {
    __u8 xx1[0x1FE - sizeof(struct elks_disk_parms)];
    struct elks_disk_parms disk_parms;
    __u8 xx2[2];		/* 0xAA55 */
} __attribute__((packed));

static int bioshd_ioctl(struct inode *, struct file *,
	unsigned int, unsigned int);

static int bioshd_open(struct inode *, struct file *);

static void bioshd_release(struct inode *, struct file *);

static int bioshd_initialized = 0;

#if 0
static struct wait_queue busy_wait;
static int revalidate_hddisk(int, int);	/* Currently not used*/
#endif

static struct biosparms bdt;

/* Useful defines for accessing the above structure. */

#define CARRY_SET (bdt.fl & 0x1)
#define BD_IRQ bdt.irq
#define BD_AX bdt.ax
#define BD_BX bdt.bx
#define BD_CX bdt.cx
#define BD_DX bdt.dx
#define BD_SI bdt.si
#define BD_DI bdt.di
#define BD_ES bdt.es
#define BD_FL bdt.fl

static struct drive_infot {		/* CHS per drive*/
    int cylinders;
    int sectors;
    int heads;
    int fdtype;				/* floppy fd_types[] index  or -1 if hd */
} drive_info[NUM_DRIVES];

/* This makes probing order more logical and
 * avoids a few senseless seeks in some cases
 */

static struct hd_struct hd[NUM_DRIVES << MINOR_SHIFT];	/* partitions start, size*/

static int access_count[NUM_DRIVES];	/* for invalidating buffers/inodes*/

static int bioshd_sizes[NUM_DRIVES << MINOR_SHIFT];	/* used only with BDEV_SIZE_CHK*/

struct drive_infot fd_types[] = {	/* AT/PS2 BIOS reported floppy formats*/
    {40,  9, 2, 0},
    {80, 15, 2, 1},
    {80,  9, 2, 2},
    {80, 18, 2, 3},
    {80, 36, 2, 4},
};

static unsigned char hd_drive_map[NUM_DRIVES] = {/* BIOS drive mappings*/
    0x80, 0x81, 0x82, 0x83,		/* hda, hdb */
    0x00, 0x01, 0x02, 0x03		/* fd0, fd1 */
};

static int _fd_count = 0;  		/* number of floppy disks */
static int _hd_count = 0;  		/* number of hard disks */

static struct wait_queue dma_wait;

static int dma_avail = 1;

static void bioshd_geninit(void);

static struct gendisk bioshd_gendisk = {
    MAJOR_NR,			/* Major number */
    "bd",			/* Major name */
    MINOR_SHIFT,		/* Bits to shift to get real from partition */
    1 << MINOR_SHIFT,		/* Number of partitions per real */
    NUM_DRIVES,			/* maximum number of real */
    bioshd_geninit,		/* init function */
    hd,				/* hd struct */
    bioshd_sizes,		/* sizes not blocksizes */
    0,				/* number */
    (void *) drive_info,	/* internal */
    NULL			/* next */
};

/* This function checks to see which hard drives are active and sets up the
 * drive_info[] table for them.  Ack this is darned confusing...
 */

#ifdef CONFIG_BLK_DEV_BHD

static unsigned short int bioshd_gethdinfo(void)
{
    unsigned short int ndrives = 0;
    int drive;
    register struct drive_infot *drivep = &drive_info[0];

    BD_AX = BIOSHD_DRIVE_PARMS;
    BD_DX = 0x80;
    BD_ES = BD_SI = 0;	/* some buggy BIOS's need this acoording to INT13 on Wiki*/
    ndrives = (call_bios(&bdt) ? 0 : BD_DX & 0xff);
    if (ndrives > NUM_DRIVES/2)
	ndrives = NUM_DRIVES/2;
    for (drive = 0; drive < ndrives; drive++) {
	BD_AX = BIOSHD_DRIVE_PARMS;
	BD_DX = drive + 0x80;
	BD_ES = BD_SI = 0;
	if (call_bios(&bdt) == 0) {
	    drivep->heads = (BD_DX >> 8) + 1;
	    drivep->sectors = BD_CX & 0x3f;
	    /* NOTE: some BIOS may underreport cylinders by 1*/
	    drivep->cylinders = (((BD_CX & 0xc0) << 2) | (BD_CX >> 8)) + 1;
	    drivep->fdtype = -1;
	    printk("bioshd: gethdinfo CHS %d,%d,%d\n", drivep->cylinders,
		drivep->heads, drivep->sectors);
	}
	drivep++;
    }
    return ndrives;
}

#endif

#ifdef CONFIG_BLK_DEV_BFD

static unsigned short int bioshd_getfdinfo(void)
{
    unsigned short int ndrives;

#ifdef CONFIG_BLK_DEV_BFD_HARD

/* Set this to match your system. Currently it's set to a two drive system:
 *
 *		1.44MB as /dev/fd0
 *	and	1.2 MB as /dev/fd1
 *
 * ndrives is number of drives in your system (either 0, 1 or 2)
 */

    ndrives = 2;

/* drive_info[] should be set *only* for existing drives;
 * comment out drive_info lines if you don't need them
 * (e.g. you have less than 2 drives)
 *
 * Enter type 4 in fd_types' brackets for unknown drive type
 * Otherwise use floppy drive type table below:
 *
 *	Type	Format
 *	~~~~	~~~~~~
 *	  0	360 KB
 *	  1	1.2 MB
 *	  2	720 KB
 *	  3	1.44 MB
 *	  4	2.88 MB or Unknown
 */

    drive_info[DRIVE_FD0] = fd_types[2];	/*  /dev/fd0    */
    drive_info[DRIVE_FD1] = fd_types[2];	/*  /dev/fd1    */

/* That's it .. you're done :-)
 *
 * Warning: drive will be reported as 2880 KB at bootup if you've set it
 * as unknown (4). Floppy probe will detect correct floppy format at each
 * change so don't bother with that
 */

#else

    register struct drive_infot *drivep = &drive_info[DRIVE_FD0];
    unsigned short int drive;

/* We get the # of drives from the BPB, which is PC-friendly
 */

#ifdef CONFIG_HW_USE_INT13_FOR_FLOPPY

/*    BD_IRQ = BIOSHD_INT;*/
    BD_AX = BIOSHD_DRIVE_PARMS;
    BD_DX = 0;			/* only the number of floppies */
    ndrives = (call_bios(&bdt) ? 0 : BD_DX & 0xff);

#else

    ndrives = (peekb(0x10, 0x40) >> 6) + 1;  /* BIOS data segment */

#endif

    for (drive = 0; drive < ndrives; drive++) {
/* If type cannot be determined correctly,
 * Type 4 should work on all AT systems
 */
	*drivep = fd_types[arch_cpu > 5 ? 3 : 0];
#ifdef CONFIG_HW_USE_INT13_FOR_FLOPPY

/* Some XT's return strange results - Al
 * The arch_cpu is a safety check
 */
	if (arch_cpu > 5) {
/*	    BD_IRQ = BIOSHD_INT;*/
	    BD_AX = BIOSHD_DRIVE_PARMS;
	    BD_DX = drive;
	    if (!call_bios(&bdt))
/*
 * AT archecture, drive type in BX
 */
		*drivep = fd_types[(BD_BX & 0xFF) - 1];
	}
#endif
	drivep++;
    }

#endif
    return ndrives;
}

#endif

static void bioshd_release(struct inode *inode, struct file *filp)
{
    int target;
    kdev_t dev = inode->i_rdev;

    sync_dev(dev);
    target = DEVICE_NR(dev);
    access_count[target]--;
    if ((target >= DRIVE_FD0) && (access_count[target] == 0)) {
	invalidate_inodes(dev);
	invalidate_buffers(dev);
    }
}

/* As far as I can tell this doesn't actually work, but we might
 * as well try it -- Some XT controllers are happy with it.. [AC]
 */

static void reset_bioshd(int drive)
{
/*    BD_IRQ = BIOSHD_INT;*/
    BD_AX = BIOSHD_RESET;
    BD_DX = drive;
    call_bios(&bdt);

/* Dont log this fail - its fine
 */
#if 0
    if (CARRY_SET)
	printk("bioshd: unable to reset.\n");
#endif
}

int read_sector(int drive, int track, int sector)
{

/* i took this code from bioshd_open() where it replicates code used
 * when new floppy probe is used
 */

    register int count = MAX_ERRS;

    do {
	/* BIOS read sector */

	BD_AX = (unsigned short int) (BIOSHD_READ | 1); /* Read 1 sector  */
	BD_BX = 0;					/* Seg offset = 0 */
	BD_ES = DMASEG;					/* Target segment */
	BD_CX = (unsigned short int) ((track << 8) | sector);
	BD_DX = drive;					/* Head 0 | drive */

	set_irq();
	if (!call_bios(&bdt)) return 0;			/* everything is OK */
	reset_bioshd(drive);
    } while (--count > 0);
    return 1;			/* error */
}

static int bioshd_open(struct inode *inode, struct file *filp)
{
    int target;
    register struct hd_struct *hdp;

    target = DEVICE_NR(inode->i_rdev);	/* >> MINOR_SHIFT */
    hdp = &hd[MINOR(inode->i_rdev)];

/* Bounds testing */

    if (bioshd_initialized == 0)
	return -ENXIO;
    if ((unsigned int)target >= NUM_DRIVES)
	return -ENXIO;
    if (hdp->start_sect == -1)
	return -ENXIO;

#if 0

/* Wait until it's free */

    while (busy[target])
	sleep_on(&busy_wait);

#endif

/* Register that we're using the device */

    access_count[target]++;

/* Check for disk type */

/* I guess it works now as it should. Tested under dosemu with 720Kb,
 * 1.2 MB and 1.44 MB floppy image and works fine - Blaz Antonic
 */

#ifdef CONFIG_BLK_DEV_BFD
    if (target >= DRIVE_FD0) {		/* the floppy drives */
	register struct drive_infot *drivep = &drive_info[target];

/* probing range can be easily extended by adding more values to these
 * two lists and adjusting for loop' parameters in line 433 and 446 (or
 * somewhere near)
 */

	static char sector_probe[5] = { 8, 9, 15, 18, 36 };
	static char track_probe[2] = { 40, 80 };
	int count;

	target &= 1;
/* The area between 32-64K is a 'scratch' area - we need a semaphore for it
 */

	while (!dma_avail)
	    sleep_on(&dma_wait);
	dma_avail = 0;

/* Try to look for an ELKS disk parameter block in the first sector.  If
 * it exists, we can obtain the disk geometry from it.
 */

	if (!read_sector(target, 0, 1)) {
	    struct elks_boot_sect __far *boot
		= (struct elks_boot_sect __far *)((__u32)DMASEG << 16);
	    struct elks_disk_parms __far *parms = &boot->disk_parms;

	    if (parms->marker[0] == 'e' && parms->marker[1] == 'L'
		&& parms->size >= offsetof(struct elks_disk_parms, size)) {
		drivep->cylinders = parms->track_max;
		drivep->sectors = parms->sect_max;
		drivep->heads = parms->head_max;

		if (drivep->cylinders != 0 && drivep->sectors != 0
		    && drivep->heads != 0) {
		    printk("fd: found valid ELKS disk parameters on /dev/fd%d "
			   "boot sector\n", target);
		    goto got_geom;
		}
	    }
	}

	printk("fd: probing disc in /dev/fd%d\n", target);

/* First probe for cylinder number. We probe on sector 1, which is
 * safe for all formats, and if we get a seek error, we assume that
 * the previous successfully probed format is the correct one.
 */

	drivep->cylinders = 0;
	count = 0;
	do {
	    if (read_sector(target, (int)track_probe[count] - 1, 1))
		break;
	    drivep->cylinders = (int)track_probe[count];
	} while (++count < 2);

/* Next, probe for sector number. We probe on track 0, which is
 * safe for all formats, and if we get a seek error, we assume that
 * the previous successfully probed format is the correct one.
 */

	drivep->sectors = 0;
	count = 0;
	do {
	    if (read_sector(target, 0, (int)sector_probe[count]))
		break;
	    drivep->sectors = (int)sector_probe[count];
	} while (++count < 5);

	drivep->heads = 2;
/* DMA code belongs out of the loop. */

      got_geom:
	dma_avail = 1;
	wake_up(&dma_wait);

/* I moved this out of for loop to prevent trashing the screen
 * with seducing (repeating) probe messages about disk types
 */

	if (drivep->cylinders == 0 || drivep->sectors == 0) {
	    *drivep = fd_types[drivep->fdtype];
	    printk("fd: Floppy drive autoprobe failed!\n");
	}
	else
	    printk("fd: /dev/fd%d probably has %d sectors, %d heads, and "
		   "%d cylinders\n",
		   target, drivep->sectors, drivep->heads, drivep->cylinders);


/*	This is not a bugfix, hence no code, but coders should be aware that
 *	multi-sector reads from this point on depend on bootsect modifying
 *	the default Disk Parameter Block in BIOS.
 *
 *	dpb[4]	should be set to a high value such as 36 so that reads can
 *		go past what is hardwired in the BIOS. 36 is the number of
 *		sectors in a 2.88 floppy track.
 *
 *		If you are booting ELKS with anything other than bootsect,
 *		you have to make equivalent arrangements.
 *
 *	0:0x78	contains the address of dpb (char dpb[12]).
 *
 *	dpb[4]	is the End of Track parameter for the 765 Floppy Disk
 *		Controller.
 *
 *	You may have to copy dpb to RAM as the original is in ROM.
 */
	hdp->start_sect = 0;
	hdp->nr_sects = ((sector_t)(drivep->sectors * drivep->heads))
				* ((sector_t)drivep->cylinders);

    }
#endif

    inode->i_size = (hdp->nr_sects) << 9;
    return 0;
}

static struct file_operations bioshd_fops = {
    NULL,			/* lseek - default */
    block_read,			/* read - general block-dev read */
    block_write,		/* write - general block-dev write */
    NULL,			/* readdir - bad */
    NULL,			/* select */
    bioshd_ioctl,		/* ioctl */
    bioshd_open,		/* open */
    bioshd_release		/* release */
#ifdef BLOAT_FS
	,
    NULL,			/* fsync */
    NULL,			/* check_media_change */
    NULL			/* revalidate */
#endif
};

#ifdef DOSHD_VERBOSE_DRIVES
#define TEMP_PRINT_DRIVES_MAX		NUM_DRIVES
#else
#ifdef CONFIG_BLK_DEV_BHD
#define TEMP_PRINT_DRIVES_MAX		(NUM_DRIVES/2)
#endif
#endif

/* Reduced code size option */
#ifdef CONFIG_SMALL_KERNEL
#undef TEMP_PRINT_DRIVES_MAX
#endif

int init_bioshd(void)
{
    register struct gendisk *ptr;
    int count;

#ifndef CONFIG_SMALL_KERNEL
    printk("hd Driver Copyright (C) 1994 Yggdrasil Computing, Inc.\n"
	   "Extended and modified for Linux 8086 by Alan Cox.\n");
#endif /* CONFIG_SMALL_KERNEL */

#ifdef CONFIG_BLK_DEV_BFD
    _fd_count = bioshd_getfdinfo();
    enable_irq(6);		/* Floppy */
#endif
#ifdef CONFIG_BLK_DEV_BHD
    _hd_count = bioshd_gethdinfo();
    if (arch_cpu > 5) {		/* PC-AT or greater */
	enable_irq(HD_IRQ);	/* AT ST506 */
	enable_irq(15);		/* AHA1542 */
    }
    else {
	enable_irq(5);		/* XT ST506 */
    }
    bioshd_gendisk.nr_real = _hd_count;
#endif /* CONFIG_BLK_DEV_BHD */

#ifdef CONFIG_BLK_DEV_BFD
#ifdef CONFIG_BLK_DEV_BHD
    printk("bioshd: %d floppy drive%s and %d hard drive%s\n",
	   _fd_count, _fd_count == 1 ? "" : "s",
	   _hd_count, _hd_count == 1 ? "" : "s");
#else
    printk("bioshd: %d floppy drive%s\n",
	   _fd_count, _fd_count == 1 ? "" : "s");
#endif
#else
#ifdef CONFIG_BLK_DEV_BHD
    printk("bioshd: %d hard drive%s\n",
	   _hd_count, _hd_count == 1 ? "" : "s");
#endif
#endif

    if (!(_fd_count + _hd_count)) return 0;

#ifdef TEMP_PRINT_DRIVES_MAX
    {
	register struct drive_infot *drivep;
	static char *unit = "kMGT";

	drivep = drive_info;
	for (count = 0; count < TEMP_PRINT_DRIVES_MAX; count++, drivep++) {
	    if (drivep->heads != 0) {
		__u32 size = ((__u32) drivep->sectors) * 5 /* 0.1 kB units */;

		size *= ((__u32) drivep->cylinders) * drivep->heads;

		/* Select appropriate unit */
		while (size > 99999 && unit[1]) {
		    debug("DBG: Size = %lu (%X/%X)\n", size, *unit, unit[1]);
		    size += 512U;
		    size /= 1024U;
		    unit++;
		}
		debug("DBG: Size = %lu (%X/%X)\n",size,*unit,unit[1]);
		printk("/dev/%cd%c: %d cylinders, %d heads, %d sectors = %lu.%u %cb\n",
		    (count < 2 ? 'h' : 'f'), (count % 2) + (count < 2 ? 'a' : '0'),
		    drivep->cylinders, drivep->heads, drivep->sectors,
		    (size/10), (int) (size%10), *unit);
	    }
	}
    }
#endif /* TEMP_PRINT_DRIVES_MAX */

    count = register_blkdev(MAJOR_NR, DEVICE_NAME, &bioshd_fops);

    if (count == 0) {
	blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;

	if (gendisk_head == NULL) {
	    bioshd_gendisk.next = gendisk_head;
	    gendisk_head = &bioshd_gendisk;
	} else {
	    for (ptr = gendisk_head; ptr->next != NULL; ptr = ptr->next)
		/* Do nothing */ ;
	    ptr->next = &bioshd_gendisk;
	    bioshd_gendisk.next = NULL;
	}
	bioshd_initialized = 1;
    } else {
	printk("bioshd: unable to register\n");
    }
    return count;
}

static int bioshd_ioctl(struct inode *inode,
			struct file *file, unsigned int cmd, unsigned int arg)
{
    register struct hd_geometry *loc = (struct hd_geometry *) arg;
    register struct drive_infot *drivep;
    int dev, err;

    if ((!inode) || !(inode->i_rdev))
	return -EINVAL;

    dev = DEVICE_NR(inode->i_rdev);
    if (dev >= ((dev < DRIVE_FD0) ? _hd_count : (DRIVE_FD0 + _fd_count)))
    	return -ENODEV;

    drivep = &drive_info[dev];
    err = -EINVAL;
    switch (cmd) {
    case HDIO_GETGEO:
	err = verify_area(VERIFY_WRITE, (void *) arg, sizeof(struct hd_geometry));
	if (!err) {
	    put_user((__u16) drivep->heads, (__u16 *) &loc->heads);
	    put_user((__u16) drivep->sectors, (__u16 *) &loc->sectors);
	    put_user((__u16) drivep->cylinders, (__u16 *) &loc->cylinders);
	    put_user((__u16) hd[MINOR(inode->i_rdev)].start_sect,
		    (__u16 *) &loc->start);
	}
    }
    return err;
}

static void do_bioshd_request(void)
{
    register struct drive_infot *drivep;
    register struct request *req;
    seg_t seg;
    unsigned char *buff;
    sector_t start, count, tmp;
    int drive, errs;
    unsigned int cylinder, head, sector, this_pass;
    unsigned short int minor, in_ax, out_ax;

#if 0
    int part;
#endif

    while (1) {

      next_block:

	/* make sure we have a valid request - Done by INIT_REQUEST */
	if (!CURRENT)
	    break;

	/* now initialize it */
	INIT_REQUEST;

	/* make sure it's still valid */
	req = CURRENT;
	if (req == NULL || (int) req->rq_sector == -1)
	    break;

	if (bioshd_initialized != 1) {
	    end_request(0);
	    continue;
	}
	minor = MINOR(req->rq_dev);

#if 0
	part = minor & ((1 << MINOR_SHIFT) - 1);
#endif

	drive = minor >> MINOR_SHIFT;
	drivep = &drive_info[drive];

/* make sure it's a disk that we are dealing with. */

	if (drive > DRIVE_FD1 || drivep->heads == 0) {
	    printk("bioshd: non-existent drive\n");
	    end_request(0);
	    continue;
	}

	drive = hd_drive_map[drive];
#ifdef BLOAT_FS
	count = req->rq_nr_sectors;
#else
	count = 2;
#endif

	start = req->rq_sector;
	buff = (unsigned char *)req->rq_buffer;
	seg = req->rq_seg;
	if (hd[minor].start_sect == -1 || start >= hd[minor].nr_sects) {
	    printk("bioshd: bad partition start=%ld sect=%ld nr_sects=%ld.\n",
		   start, hd[minor].start_sect, hd[minor].nr_sects);
	    end_request(0);
	    continue;
	}
	start += hd[minor].start_sect;

	while (count > 0) {
	    sector = (unsigned int) ((start % (sector_t)drivep->sectors) + 1);
	    tmp = start / (sector_t)drivep->sectors;
	    head = (unsigned int) (tmp % (sector_t)drivep->heads);
	    cylinder = (unsigned int) (tmp / (sector_t)drivep->heads);
	    this_pass = drivep->sectors - sector + 1;
	/* Fix for weird BIOS behavior with 720K floppy (issue #39) */
	    if (this_pass < 3) this_pass = 1;
	/* End of fix */
	    if ((sector_t)this_pass > count) this_pass = (unsigned int) count;

	    errs = MAX_ERRS;	/* BIOS disk reads should be retried at least three times */
	    do {
		unsigned rq_start_dma_page, rq_end_dma_page;
		int need_dma_seg;

		while (!dma_avail) sleep_on(&dma_wait);
		dma_avail = 0;

		/* Try to gauge if we can do the read/write directly on the actual source/
		   target buffer without crossing a 64 KiB DMA boundary

		   If not, then do the read/write by way of DMASEG:0 */
		rq_start_dma_page = (seg + ((__u16) buff >> 4)) >> 12;
		rq_end_dma_page = (seg + ((__u16) (buff + this_pass * 512 - 1) >> 4)) >> 12;
		need_dma_seg = (rq_start_dma_page != rq_end_dma_page);

		if (need_dma_seg) {
		    if (this_pass > (DMASEGSZ >> 9)) {
			/* Then again, if we limit our read/write request to DMASEGSZ bytes,
			   it may turn out that we no longer need DMASEG:0... (!) */
			this_pass = (DMASEGSZ >> 9);
			rq_end_dma_page = (seg + ((__u16) (buff + DMASEGSZ - 1) >> 4)) >> 12;
			need_dma_seg = (rq_start_dma_page != rq_end_dma_page);
		    }
		}

		if (need_dma_seg) {
		    if (req->rq_cmd == WRITE)
			fmemcpyb(NULL, DMASEG, buff, seg, (this_pass << 9));
		    BD_BX = 0;
		    BD_ES = DMASEG;
		} else {
		    BD_BX = (__u16) buff;
		    BD_ES = seg;
		}
		BD_AX = (req->rq_cmd == WRITE ? BIOSHD_WRITE : BIOSHD_READ) | this_pass;
		BD_CX = (unsigned short int)
			    ((cylinder << 8) | ((cylinder >> 2) & 0xc0) | sector);
		BD_DX = (head << 8) | drive;
		debug("cylinder=%d head=%d sector=%d blocks=%d drive=0x%x CMD=%d\n",
		    cylinder, head, sector, this_pass, drive, req->rq_cmd);
		in_ax = BD_AX;
		out_ax = 0;

		if (call_bios(&bdt)) {
		    out_ax = BD_AX;
		    reset_bioshd(drive); /* controller should be reset upon error detection */
		} else if (need_dma_seg && req->rq_cmd == READ)
		    fmemcpyb(buff, seg, NULL, DMASEG, (word_t)(this_pass << 9));

		dma_avail = 1;
		wake_up(&dma_wait);
	    } while (out_ax && --errs);	/* On error, retry up to MAX_ERRS times */

	    if (out_ax) {
		printk("bioshd: error: out AX=0x%04X in AX=0x%04X "
		       "ES:BX=0x%04X:0x%04X\n", out_ax, in_ax, BD_ES, BD_BX);
		end_request(0);
		goto next_block;
	    }

	    count -= this_pass;
	    start += this_pass;
	    buff += this_pass * 512;
	}

/* satisfied that request */

	end_request(1);
    }
}

#if 0
#define DEVICE_BUSY busy[target]
#endif

#define USAGE access_count[target]
#define CAPACITY ((sector_t)drive_info[target].heads*drive_info[target].sectors*drive_info[target].cylinders)

/* We assume that the the bios parameters do not change,
 * so the disk capacity will not change
 */

#undef MAYBE_REINIT
#define GENDISK_STRUCT bioshd_gendisk

/* This routine is called to flush all partitions and partition tables
 * for a changed cdrom drive, and then re-read the new partition table.
 * If we are revalidating a disk because of a media change, then we
 * enter with usage == 0.  If we are using an ioctl, we automatically
 * have usage == 1 (we need an open channel to use an ioctl :-), so
 * this is our limit.
 */

#if 0			/* Currently not used, removing for size. */
#ifndef MAYBE_REINIT
#define MAYBE_REINIT
#endif

static int revalidate_hddisk(int dev, int maxusage)
{
    register struct gendisk *gdev;
    int i, major, max_p, start, target;

    target = DEVICE_NR(dev);
    gdev = &GENDISK_STRUCT;
    clr_irq();
    if (DEVICE_BUSY || USAGE > maxusage) {
	set_irq();
	return -EBUSY;
    }
    DEVICE_BUSY = 1;
    set_irq();
    max_p = gdev->max_p;
    start = target << gdev->minor_shift;
    major = MAJOR_NR << 8;
    for (i = max_p - 1; i >= 0; i--) {
	sync_dev(major | start | i);
	invalidate_inodes(major | start | i);
	invalidate_buffers(major | start | i);
	gdev->part[start + i].start_sect = 0;
	gdev->part[start + i].nr_sects = 0;
    };
    MAYBE_REINIT;
    gdev->part[start].nr_sects = CAPACITY;
    resetup_one_dev(gdev, target);
    DEVICE_BUSY = 0;
    wake_up(&busy_wait);
    return 0;
}
#endif

static void bioshd_geninit(void)
{
    register struct drive_infot *drivep;
    register struct hd_struct *hdp = hd;
    int i;

    drivep = drive_info;
    for (i = 0; i < NUM_DRIVES << MINOR_SHIFT; i++) {
	if ((i & ((1 << MINOR_SHIFT) - 1)) == 0) {
	    hdp->nr_sects = (sector_t) drivep->sectors *
		drivep->heads * drivep->cylinders;
	    hdp->start_sect = 0;
	    drivep++;
	} else {
	    hdp->nr_sects = 0;
	    hdp->start_sect = -1;
	}
	hdp++;
    }

#if 0
    blksize_size[MAJOR_NR] = 1024;	/* Currently unused */
#endif

}

/* convert a bios drive number to a bioshd kdev_t*/
kdev_t bioshd_conv_bios_drive(unsigned int biosdrive)
{
    int minor;
    int partition = 0;
    extern int boot_partition;

    if (biosdrive & 0x80) {		/* hard drive*/
	minor = biosdrive & 0x03;
	partition = boot_partition;	/* saved from add_partition()*/
    } else
	minor = (biosdrive & 0x03) + DRIVE_FD0;
    return MKDEV(BIOSHD_MAJOR, (minor << MINOR_SHIFT) + partition);
}

#endif
