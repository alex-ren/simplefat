/*
 * io.c
 *
 *  Created on: Jan 24, 2012
 *      Author: Zhiqiang Ren
 *      Email: aren@bu.edu
 */

#include <linux/capability.h>
#include <linux/module.h>
#include <linux/mount.h>
#include <linux/time.h>
#include <linux/buffer_head.h>
#include <linux/writeback.h>
#include <linux/backing-dev.h>
#include <linux/blkdev.h>
#include <linux/fsnotify.h>
#include <linux/security.h>


struct block_holder
{
    struct buffer_head bh;
    struct page *page;
};

static struct kmem_cache *sfat_cache_blkholder = 0;

static void init_once(void *foo)
{
    struct block_holder *bh = (struct block_holder *)foo;

    memset(bh, 0, sizeof(*bh));
    INIT_LIST_HEAD(&bh->bh.b_assoc_buffers);
}

/*
 * return: 0 => success
 *         -ENOMEM
 */
int __init sfat_blkholder_cache_init(void)
{
    sfat_cache_blkholder = kmem_cache_create("sfat_blkholder_cache",
                            sizeof(struct block_holder),
                            0, (SLAB_RECLAIM_ACCOUNT|
                            SLAB_MEM_SPREAD),
                            init_once);
    if (sfat_cache_blkholder == NULL)
    {
        return -ENOMEM;
    }

    return 0;
}

void sfat_blkholder_cache_destroy(void)
{
    kmem_cache_destroy(sfat_cache_blkholder);
    sfat_cache_blkholder = 0;
}

struct block_holder *sfat_blkholder_alloc(void)
{
    struct block_holder *bh = 0;
    struct page *page = 0;

    bh = kmem_cache_alloc(sfat_cache_blkholder, GFP_NOFS);
    if (!bh)
    {
        return NULL;
    }

    page = __page_cache_alloc(GFP_NOFS);
    if (!page)
    {
        kmem_cache_free(sfat_cache_blkholder, bh);
        return NULL;
    }

    bh->page = page;

    /* ================*/
//       bh->bh.b_bdev = NULL;
//       bh->bh.b_this_page = NULL;
//       bh->bh.b_blocknr = -1;
//       bh->bh.b_state = 0;
//       atomic_set(&(bh->bh.b_count), 0);
//       bh->bh.b_private = NULL;
//
//
//       /* Link the buffer to its page */
//       set_bh_page(&bh->bh, page, 0);
//       init_buffer(&bh->bh, NULL, NULL);
//
//       set_buffer_mapped(&bh->bh);
       /* ================*/


    return bh;
}

void sfat_blkholder_free(struct block_holder *bh)
{
    page_cache_release(bh->page);
    kmem_cache_free(sfat_cache_blkholder, bh);
}

char *sfat_blkholder_get_data(struct block_holder *bh)
{
    return bh->bh.b_data;
}

/*
 * blk_holder: must be valid, no check inside the function
 * blk_sz: must be multiple of 512 and less then 4096
 *      if blk_sz == 0 then use the def
 *
 * return: 0 => success
 *         -EIO
 */
int read_block(struct block_device *bdev, struct block_holder *blk_holder,
                            size_t blk_sz, size_t blk_no)
{
    struct page *page = 0;
    struct buffer_head *bh = 0;

    page = blk_holder->page;
    bh = &blk_holder->bh;

    if (blk_sz == 0)
    {
        // size supported by the device
        blk_sz = bdev_logical_block_size(bdev);
    }

    // lock_page(page);  // seems unnecessary so far

    // rzq: quoted from alloc_page_buffers()
    bh->b_bdev = NULL;
    bh->b_this_page = NULL;
    bh->b_blocknr = -1;
    // bh->b_state = 0;
    // atomic_set(&bh->b_count, 0);
    bh->b_private = NULL;
    bh->b_size = blk_sz;

    /* Link the buffer to its page */
    set_bh_page(bh, page, 0);
    init_buffer(bh, NULL, NULL);

    bh->b_bdev = bdev;
    bh->b_blocknr = blk_no;
    // set_buffer_mapped(bh);

    // rzq: prepare for end_buffer_read_sync() (see below)
    lock_buffer(bh);  // lock the buffer head, end_buffer_read_sync (see below) contains unlock
    get_bh(bh);  // end_buffer_read_sync (see below) contains put_bh
    clear_buffer_uptodate(bh);  // clear the bit for future test of whether the read succeeds

    bh->b_end_io = end_buffer_read_sync;

    submit_bh(READ, bh);
    wait_on_buffer(bh);
    // unlock_page(page);

    if (buffer_uptodate(bh))
    {
        return 0;
    }
    else
    {
        return -EIO;
    }

}

/*
 * blk_holder: must be valid, no check inside the function
 * blk_sz: must be multiple of 512 and less then 4096
 *      if blk_sz == 0 then use the def
 *
 * return: 0 => success
 *         -EIO
 */
int write_block(struct block_device *bdev, struct block_holder *blk_holder,
                            size_t blk_sz, size_t blk_no)
{

    struct page *page = 0;
    struct buffer_head *bh = 0;

    page = blk_holder->page;
    bh = &blk_holder->bh;

    if (blk_sz == 0)
    {
        // size supported by the device
        blk_sz = bdev_logical_block_size(bdev);
    }



//    bh->b_bdev = NULL;
//    bh->b_this_page = NULL;
//    bh->b_blocknr = -1;
//    bh->b_state = 0;
//    atomic_set(&bh->b_count, 0);
//    bh->b_private = NULL;
//    bh->b_size = blk_sz;

    /* Link the buffer to its page */
    set_bh_page(bh, page, 0);
    init_buffer(bh, NULL, NULL);

    bh->b_bdev = bdev;
    bh->b_blocknr = blk_no;
    set_buffer_mapped(bh);









    // lock_page(page);  // seems unnecessary so far

    lock_buffer(bh);  // lock the buffer head, end_buffer_write_sync (see below) contains unlock
    get_bh(bh);  // end_buffer_read_sync (see below) contains put_bh
    clear_buffer_uptodate(bh);  // clear the bit for future test of whether the read succeeds

    bh->b_end_io = end_buffer_write_sync;
    submit_bh(WRITE_BARRIER, bh);
    wait_on_buffer(bh);

    // unlock_page(page);

    if (buffer_uptodate(bh))
    {
        return 0;
    }
    else
    {
        return -EIO;
    }

}




