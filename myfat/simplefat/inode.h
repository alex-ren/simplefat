#ifndef __SFAT_INODE_H
#define __SFAT_INODE_H

#include <linux/fs.h>
#include "sfat.h"

/*
 * SFAT file system inode data in memory
 */
struct sfat_inode_info {

    size_t i_start;        /* first cluster or SFAT_ENTRY_FREE */
    int i_attrs;           /* SFAT attribute */
//    int i_logstart;     /* logical first cluster */

    loff_t i_pos;       /* position of directory entry
                        (in the volume) or 0 */  // in Byte
    struct inode vfs_inode;  /* The real inode for VFS */
};


struct inode *sfat_alloc_inode(struct super_block *sb);

void sfat_destroy_inode(struct inode *sb);

void sfat_delete_inode(struct inode *sb);

void sfat_clear_inode(struct inode *sb);

int sfat_read_root(struct inode *inode);

// ===============================

int sfat_create(struct inode *dir, struct dentry *dentry, int mode,
            struct nameidata *nd);

// ===============================

// Get the sfat_inode_info which contains the inode
static inline struct sfat_inode_info *SFAT_I(struct inode *inode)
{
    return container_of(inode, struct sfat_inode_info, vfs_inode);
}




int sfat_inodeinfo_cache_init(void);

void sfat_inodeinfo_cache_destroy(void);



int nextCluster(struct sfat_fs_info *fs, struct block_device *bdev, size_t cls, size_t *next);

int sfat_count_subdirs(struct inode *inode);

#endif


