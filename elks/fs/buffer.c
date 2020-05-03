#include <linuxmt/types.h>
#include <linuxmt/sched.h>
#include <linuxmt/kernel.h>
#include <linuxmt/major.h>
#include <linuxmt/string.h>
#include <linuxmt/mm.h>
#include <linuxmt/errno.h>
#include <linuxmt/debug.h>

#include <arch/system.h>
#include <arch/segment.h>
#include <arch/io.h>
#include <arch/irq.h>

// Number of external buffers specified in config by CONFIG_FS_NR_EXT_BUFFERS

// Number of internal L1 buffers
// used to map/copy external L2 buffers to/from kernel data segment

#ifdef CONFIG_FS_FAT
#define NR_MAPBUFS  12
#else
#define NR_MAPBUFS  8
#endif

// Number of internal L1 buffers

#ifdef CONFIG_FS_EXTERNAL_BUFFER
#define NR_BUFFERS CONFIG_FS_NR_EXT_BUFFERS
#else
#define NR_BUFFERS NR_MAPBUFS
#endif

// Buffer heads (internal or external)

static struct buffer_head buffer_heads[NR_BUFFERS];

// Internal L1 buffers

static char L1buf[NR_MAPBUFS][BLOCK_SIZE];

// Buffer cache

static struct buffer_head *bh_lru = buffer_heads;
static struct buffer_head *bh_llru = buffer_heads;

// External L2 buffers are allocated within global memory segments
// each segment being up to 64K in size for segment register addressing
// Total global memory used is (BLOCK_SIZE * CONFIG_FS_NR_EXT_BUFFERS)

#ifdef CONFIG_FS_EXTERNAL_BUFFER
static struct buffer_head *L1map[NR_MAPBUFS];	/* L1 indexed pointer to L2 buffer */
static struct wait_queue L1wait;		/* Wait for a free L1 buffer area */
static int lastL1map;
#endif

/* Uncomment next line to debug free buffer_head count */
/*#define DEBUG_FREE_BH_COUNT */

#ifdef DEBUG_FREE_BH_COUNT
static int nr_free_bh = NR_BUFFERS;
#define DCR_COUNT(bh) if(!(--bh->b_count))nr_free_bh++
#define INR_COUNT(bh) if(!(bh->b_count++))nr_free_bh--
#define CLR_COUNT(bh) if(bh->b_count)nr_free_bh++
#define SET_COUNT(bh) if(--nr_free_bh < 0) { \
	printk("VFS: get_free_buffer: bad free buffer head count.\n"); \
	nr_free_bh = 0; \
    }
#else
#define DCR_COUNT(bh) (bh->b_count--)
#define INR_COUNT(bh) (bh->b_count++)
#define CLR_COUNT(bh)
#define SET_COUNT(bh)
#endif

static void put_last_lru(register struct buffer_head *bh)
{
    register struct buffer_head *bhn;

    if (bh_llru != bh) {

	bhn = bh->b_next_lru;
	if ((bhn->b_prev_lru = bh->b_prev_lru))	/* Unhook */
	    bh->b_prev_lru->b_next_lru = bhn;
	else					/* Alter head */
	    bh_lru = bhn;
	/*
	 *      Put on lru end
	 */
	bh->b_next_lru = NULL;
	(bh->b_prev_lru = bh_llru)->b_next_lru = bh;
	bh_llru = bh;
    }
}

void buffer_init(void)
{
#ifdef CONFIG_FS_EXTERNAL_BUFFER
    struct buffer_head *bh = buffer_heads;
    segment_s *seg = 0;		/* init stops compiler warning*/
    int bufs_to_alloc = CONFIG_FS_NR_EXT_BUFFERS;
    int nbufs = 0;
    int i = NR_MAPBUFS;

    do {
	L1map[--i] = NULL;
    } while (i > 0);
#else
	char * p = L1buf;
#endif

    goto buf_init;
    do {
	bh->b_next_lru = bh->b_prev_lru = bh;
	put_last_lru(bh);
      buf_init:

#ifdef CONFIG_FS_EXTERNAL_BUFFER
	if (--nbufs <= 0) {
	    /* allocate buffers in 64k chunks so addressable with segment/offset*/
	    if ((nbufs = bufs_to_alloc) > 64)
		nbufs = 64;
	    bufs_to_alloc -= nbufs;
	    seg = seg_alloc (nbufs << (BLOCK_SIZE_BITS - 4), SEG_FLAG_EXTBUF);
	    //if (!seg) panic("No extbuf mem");
	}
	bh->b_ds = seg->base;
	bh->b_data = (char *)0;		/* L1 buffer cache is reserved! */
	bh->b_mapcount = 0;
	bh->b_offset = i & 63;		/* used to compute L2 location*/
	bh->b_num = i++;
#else
	bh->b_data = p;
	p += BLOCK_SIZE;
#endif

    } while (++bh < &buffer_heads[NR_BUFFERS]);
}

/*
 *	Wait on a buffer
 */

void wait_on_buffer(register struct buffer_head *bh)
{
    while (buffer_locked(bh)) {
	INR_COUNT(bh);
	sleep_on(&bh->b_wait);
	DCR_COUNT(bh);
    }
}

// Lock buffer in memory
// when driver is working on it
// or client is reading from / writing to it

// TODO: replace by a 'lock_t'

void lock_buffer(register struct buffer_head *bh)
{
    wait_on_buffer(bh);
    bh->b_lock = 1;
}

void unlock_buffer(register struct buffer_head *bh)
{
    bh->b_lock = 0;
    wake_up(&bh->b_wait);
}

void invalidate_buffers(kdev_t dev)
{
    register struct buffer_head *bh = bh_llru;

    do {
	if (bh->b_dev != dev) continue;
	wait_on_buffer(bh);
/*	if (bh->b_dev != dev)*/	/* This will never happen */
/*	    continue;*/
	if (bh->b_count) continue;
	bh->b_uptodate = 0;
	bh->b_dirty = 0;
	unlock_buffer(bh);
    } while ((bh = bh->b_prev_lru) != NULL);
}

static void sync_buffers(kdev_t dev, int wait)
{
    register struct buffer_head *bh = bh_llru;

    do {
	/*
	 *      Skip clean buffers.
	 */
	if ((dev && (bh->b_dev != dev)) || (buffer_clean(bh)))
	   continue;

	/*
	 *      Locked buffers..
	 *
	 *      If buffer is locked; skip it unless wait is requested
	 *      AND pass > 0.
	 */
	if (buffer_locked(bh)) {
	    if (!wait) continue;
	    wait_on_buffer(bh);
	}

	/*
	 *      Do the stuff
	 */
	INR_COUNT(bh);
	ll_rw_blk(WRITE, bh);
	DCR_COUNT(bh);
    } while ((bh = bh->b_prev_lru) != NULL);
}

static struct buffer_head *get_free_buffer(void)
{
    register struct buffer_head *bh;

    bh = bh_lru;
#ifdef CONFIG_FS_EXTERNAL_BUFFER
    while (bh->b_count || bh->b_dirty || buffer_locked (bh) || bh->b_data) {
#else
    while (bh->b_count || bh->b_dirty || buffer_locked (bh)) {
#endif
	if ((bh = bh->b_next_lru) == NULL) {
	    sync_buffers(0, 0);
	    bh = bh_lru;
	}
    }
    put_last_lru(bh);
    bh->b_uptodate = 0;
    bh->b_count = 1;
    SET_COUNT(bh)
    return bh;
}

/*
 * Release a buffer head
 */

void __brelse(register struct buffer_head *buf)
{
    wait_on_buffer(buf);

    if (buf->b_count <= 0) panic("brelse");
#if 0
    if (!--buf->b_count)
	wake_up(&bufwait);
#else
    DCR_COUNT(buf);
#endif
}

/*
 * bforget() is like brelse(), except it removes the buffer
 * data validity.
 */
#if 0
void __bforget(struct buffer_head *buf)
{
    wait_on_buffer(buf);
    buf->b_dirty = 0;
    DCR_COUNT(buf);
    buf->b_dev = NODEV;
    wake_up(&bufwait);
}
#endif

/* Turns out both minix_bread and bread do this, so I made this a function
 * of it's own... */

struct buffer_head *readbuf(register struct buffer_head *bh)
{
    if (!buffer_uptodate(bh)) {
	ll_rw_blk(READ, bh);
	wait_on_buffer(bh);
	if (!buffer_uptodate(bh)) {
	    brelse(bh);
	    bh = NULL;
	}
    }
    return bh;
}

static struct buffer_head *find_buffer(kdev_t dev, block_t block)
{
    register struct buffer_head *bh = bh_llru;

    do {
	if (bh->b_blocknr == block && bh->b_dev == dev) break;
    } while ((bh = bh->b_prev_lru) != NULL);
    return bh;
}

struct buffer_head *get_hash_table(kdev_t dev, block_t block)
{
    register struct buffer_head *bh;

    if ((bh = find_buffer(dev, block)) != NULL) {
	INR_COUNT(bh);
	wait_on_buffer(bh);
    }
    return bh;
}

/*
 * Ok, this is getblk, and it isn't very clear, again to hinder
 * race-conditions. Most of the code is seldom used, (ie repeating),
 * so it should be much more efficient than it looks.
 *
 * The algorithm is changed: hopefully better, and an elusive bug removed.
 *
 * 14.02.92: changed it to sync dirty buffers a bit: better performance
 * when the filesystem starts to get full of dirty blocks (I hope).
 */

struct buffer_head *getblk(kdev_t dev, block_t block)
{
    register struct buffer_head *bh;
    register struct buffer_head *n_bh;

    /* If there are too many dirty buffers, we wake up the update process
     * now so as to ensure that there are still clean buffers available
     * for user processes to use (and dirty) */

    n_bh = NULL;
    goto start;
    do {
	/*
	 * Block not found. Create a buffer for this job.
	 */
	n_bh = get_free_buffer();	/* This function may sleep and someone else */
      start:				/* can create the block */
	if ((bh = find_buffer(dev, block)) != NULL) goto found_it;
    } while (n_bh == NULL);
    bh = n_bh;				/* Block not found, use the new buffer */
/* OK, FINALLY we know that this buffer is the only one of its kind,
 * and that it's unused (b_count=0), unlocked (buffer_locked=0), and clean
 */
    bh->b_dev = dev;
    bh->b_blocknr = block;
    bh->b_seg = kernel_ds;
    goto return_it;

  found_it:
    if (n_bh != NULL) {
	CLR_COUNT(n_bh)
	n_bh->b_count = 0;	/* Release previously created buffer head */
    }
    INR_COUNT(bh);
    wait_on_buffer(bh);
    if (buffer_clean(bh) && buffer_uptodate(bh))
	put_last_lru(bh);

  return_it:
    return bh;
}

/*
 * bread() reads a specified block and returns the buffer that contains
 * it. It returns NULL if the block was unreadable.
 */

struct buffer_head *bread(kdev_t dev, block_t block)
{
    return readbuf(getblk(dev, block));
}

#if 0

/* NOTHING is using breada at this point, so I can pull it out... Chad */

struct buffer_head *breada(kdev_t dev,
			   block_t block,
			   int bufsize,
			   unsigned int pos, unsigned int filesize)
{
    register struct buffer_head *bh, *bha;
    int i, j;

    if (pos >= filesize)
	return NULL;

    if (block < 0)
	return NULL;
    bh = getblk(dev, block);

    if (buffer_uptodate(bh))
	return bh;

    bha = getblk(dev, block + 1);
    if (buffer_uptodate(bha)) {
	brelse(bha);
	bha = NULL;
    } else {
	/* Request the read for these buffers, and then release them */
	ll_rw_blk(READ, bha);
	brelse(bha);
    }
    /* Wait for this buffer, and then continue on */
    wait_on_buffer(bh);
    if (buffer_uptodate(bh))
	return bh;
    brelse(bh);
    return NULL;
}

#endif

void mark_buffer_uptodate(struct buffer_head *bh, int on)
{
    flag_t flags;

    save_flags(flags);
    clr_irq();
    bh->b_uptodate = on;
    restore_flags(flags);
}

void fsync_dev(kdev_t dev)
{
    sync_buffers(dev, 0);
    sync_supers(dev);
    sync_inodes(dev);
    sync_buffers(dev, 1);
}

void sync_dev(kdev_t dev)
{
    sync_buffers(dev, 0);
    sync_supers(dev);
    sync_inodes(dev);
    sync_buffers(dev, 0);
}

int sys_sync(void)
{
    fsync_dev(0);
    return 0;
}

#ifdef CONFIG_FS_EXTERNAL_BUFFER
/* map_buffer copies a buffer into L1 buffer space. It will freeze forever
 * before failing, so it can return void.  This is mostly 8086 dependant,
 * although the interface is not.
 */
void map_buffer(register struct buffer_head *bh)
{
    struct buffer_head *bmap;
    int i;

    /* If buffer is already mapped, just increase the refcount and return */
    if (bh->b_data /*|| bh->b_seg != kernel_ds*/) {
	if (!bh->b_mapcount)
	    debug("REMAP: %d\n", bh->b_num);
	goto end_map_buffer;
    }

    i = lastL1map;
    /* search for free L1 buffer or wait until one is available*/
    for (;;) {
	if (++i >= NR_MAPBUFS) i = 0;
	debug("map:   %d try %d\n", bh->b_num, i);

	/* First check for the trivial case, to avoid dereferencing a null pointer */
	if (!(bmap = L1map[i]))
	    break;

	/* L1 with zero count can be unmapped and reused for this request*/
	if (bmap->b_mapcount < 0)
		printk("map_buffer: %d BAD mapcount %d\n", bmap->b_num, bmap->b_mapcount);
	if (!bmap->b_mapcount) {
	    debug("UNMAP: %d <- %d\n", bmap->b_num, i);

	    /* Unmap/copy L1 to L2 */
	    fmemcpyb((byte_t *) (bmap->b_offset << BLOCK_SIZE_BITS), bmap->b_ds,
		     (byte_t *) bmap->b_data, kernel_ds, BLOCK_SIZE);
	    bmap->b_data = 0;
	    break;		/* success */
	}
	if (i == lastL1map) {
	    /* no free L1 buffers, must wait for L1 unmap_buffer*/
	    debug("MAPWAIT: %d\n", bh->b_num);
	    sleep_on(&L1wait);
	}
    }

    /* Map/copy L2 to L1 */
    lastL1map = i;
    L1map[i] = bh;
    bh->b_data = (char *)L1buf + (i << BLOCK_SIZE_BITS);
    if (bh->b_uptodate) {
	fmemcpyb((byte_t *) bh->b_data, kernel_ds,
		 (byte_t *) (bh->b_offset << BLOCK_SIZE_BITS), bh->b_ds, BLOCK_SIZE);
    }
    debug("MAP:   %d -> %d\n", bh->b_num, i);
  end_map_buffer:
    bh->b_mapcount++;
}

/* unmap_buffer decreases bh->b_mapcount, and wakes up anyone waiting over
 * in map_buffer if it's decremented to 0... this is a bit of a misnomer,
 * since the unmapping is actually done in map_buffer to prevent frivoulous
 * unmaps if possible.
 */
void unmap_buffer(register struct buffer_head *bh)
{
    if (bh) {
	if (bh->b_mapcount <= 0) {
	    printk("unmap_buffer: %d BAD mapcount %d\n", bh->b_num, bh->b_mapcount);
	    bh->b_mapcount = 0;
	} else if (--bh->b_mapcount == 0) {
	    debug("unmap: %d\n", bh->b_num);
	    wake_up(&L1wait);
	} else
	    debug("unmap_buffer: %d mapcount %d\n", bh->b_num, bh->b_mapcount+1);
    }
}

void unmap_brelse(register struct buffer_head *bh)
{
    if (bh) {
	unmap_buffer(bh);
	__brelse(bh);
    }
}
#endif /* CONFIG_FS_EXTERNAL_BUFFER*/
