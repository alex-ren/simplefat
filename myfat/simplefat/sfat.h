#ifndef __SFAT_H
#define __SFAT_H

#include <linux/buffer_head.h>
#include <linux/string.h>
#include <linux/nls.h>
#include <linux/fs.h>
#include <linux/mutex.h>

#include "sfat_fs.h"

#define FAT_ERRORS_CONT     1      /* ignore error and continue */
#define FAT_ERRORS_PANIC    2      /* panic on error */
#define FAT_ERRORS_RO       3      /* remount r/o on error */


/* on-disk position (in byte) of directory entry for root */
/* The one should be a special number different from other feasible position */
#define SFAT_ROOT_DIRENTRY_POS 0

// copied from fat
struct sfat_mount_options {
    uid_t fs_uid;
    gid_t fs_gid;
    unsigned short fs_fmask;
    unsigned short fs_dmask;
//    unsigned short codepage;  /* Codepage for shortname conversions */
//    char *iocharset;          /* Charset used for filename input/display */
//    unsigned short shortname; /* flags for shortname display/create rule */
//    unsigned char name_check; /* r = relaxed, n = normal, s = strict */
    unsigned char errors;     /* On error: continue, panic, remount-ro */
//    unsigned short allow_utime;/* permission for setting the [am]time */
//    unsigned quiet:1,         /* set = fake successful chmods and chowns */
//         showexec:1,      /* set = only set x bit for com/exe/bat */
//         sys_immutable:1, /* set = system files are immutable */
//         dotsOK:1,        /* set = hidden and system files are named '.filename' */
//         isvfat:1,        /* 0=no vfat long filename support, 1=vfat support */
//         utf8:1,      /* Use of UTF-8 character set (Default) */
//         unicode_xlate:1, /* create escape sequences for unhandled Unicode */
//         numtail:1,       /* Does first alias have a numeric '~1' type tail? */
//         flush:1,     /* write things quickly */
//         nocase:1,    /* Does this need case conversion? 0=need case conversion*/
//         usefree:1,   /* Use free_clusters for FAT32 */
//         tz_utc:1,    /* Filesystem timestamps are in UTC */
//         rodir:1;     /* allow ATTR_RO for directory */
};

struct sfat_fs_info {
    unsigned short block_size;      /* block size in bytes */
    unsigned short block_bits;    /* log2(block_size) */

    unsigned short sector_size;     /* sector size in bytes */
    unsigned short sector_bits;    /* log2(sector_size) */

    unsigned short cluster_size;    /* cluster size in bytes */
    unsigned short cluster_bits;    /* log2(cluster_size) */

    unsigned short sec_per_clus;    /* sectors/cluster */
    unsigned short blk_per_sec;     /* blocks/sector */
    unsigned short blk_per_clus;    /* blocks/cluster */

    unsigned short dirent_per_blk;  /* dir entries/block */

    unsigned short blk_per_sec_bits;
    unsigned short blk_per_clus_bits;

    unsigned long fat_start_sec;        /* start pos of fat in sector */
    unsigned long fat_start_blk;        /* start pos of fat in block */

    unsigned long fat_length_sec;       /* length of one fat in sectors */
    unsigned long fat_length_blk;       /* length of one fat in blocks */

    unsigned char fats;             /* no. of FATs */
    
    unsigned long data_start_sec;       /* first data sector in sectors */
    unsigned long data_start_blk;       /* first data sector in blocks */

    unsigned long clusters;         /* no. of clusters in the data area */
    
    unsigned long root_cluster_cls;    /* cluster no. of the root directory */
};

/*
 * Desc:
 *
 * In:
 *   cls: cluster no.
 *
 * Out:
 *   The block no. in the volume.
 *
 */
static inline size_t CLS_TO_BLK(struct sfat_fs_info *fs, size_t cls)
{
    return fs->data_start_blk + (cls << (fs->cluster_bits - fs->block_bits));
}

/*
 * Desc:
 *
 * In:
 *   cls: cluster no.
 *   blk: blk no. in the cluster
 *   offset: offset (in byte) in the block
 *
 * Out:
 *   The offset (in byte) in the volume.
 *
 */
static inline loff_t form_dir_entry_pos(struct sfat_fs_info *fs, size_t cls, size_t blk, size_t offset)
{
    return ((CLS_TO_BLK(fs, cls) + blk) << fs->block_bits) + offset;
}

struct sfat_sb_info {
    struct sfat_fs_info fs_info;

    unsigned long root_size;       /* size of root directory in cluster */
    
    // so far the following is unused
    struct mutex fat_lock;
    spinlock_t inode_hash_lock;
    // struct hlist_head inode_hashtable[FAT_HASH_SIZE];
    
    struct nls_table *nls_disk;  /* Codepage used on disk */
    struct nls_table *nls_io;    /* Charset used for input and display */
    
    // struct inode *sfat_inode;
    struct sfat_mount_options options;
};


static inline struct sfat_sb_info *SFAT_SB(struct super_block *sb)
{
    return sb->s_fs_info;
}

/* Convert attribute bits and a mask to the UNIX mode. */
/*
 * attrs: attributes pertaining to SFAT, e.g. SFAT_ATTR_DIR
 * mode_t: mode of Linux e.g. S_IFDIR, S_IWUGO
 */
static inline mode_t sfat_make_mode(struct sfat_sb_info *sbi,
                   u8 attrs, mode_t mode)
{
//    if (attrs & ATTR_RO && !((attrs & ATTR_DIR) && !sbi->options.rodir))
//        mode &= ~S_IWUGO;

    mode_t ret = 0;
    if (attrs & SFAT_ATTR_DIR)
    {

        ret = (mode & ~sbi->options.fs_dmask) | S_IFDIR;
        printk(KERN_INFO "sfat_make_mode S_IFDIR mode is %d\n", ret);
        return ret;
    }
    else
    {
        return (mode & ~sbi->options.fs_fmask) | S_IFREG;
    }
}



#endif



