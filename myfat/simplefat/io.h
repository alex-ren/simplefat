/*
 * io.h
 *
 *  Created on: Jan 24, 2012
 *      Author: Zhiqiang Ren
 *      Email: aren@bu.edu
 */

#ifndef __SFAT_IO_H
#define __SFAT_IO_H

#include "sfat_fs.h"

int sfat_blkholder_cache_init(void);

void sfat_blkholder_cache_destroy(void);

struct block_holder *sfat_blkholder_alloc(void);

void sfat_blkholder_free(struct block_holder *bh);

char *sfat_blkholder_get_data(struct block_holder *bh);

int read_block(struct block_device *bdev, struct block_holder *blk_holder,
                            size_t blk_sz, size_t blk_no);

int write_block(struct block_device *bdev, struct block_holder *blk_holder,
                            size_t blk_sz, size_t blk_no);


#endif


