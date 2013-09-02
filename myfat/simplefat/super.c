/*
 *  fs/rzqfat/simplefat/super.c
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/seq_file.h>
#include <linux/pagemap.h>
#include <linux/mpage.h>
#include <linux/buffer_head.h>
#include <linux/exportfs.h>
#include <linux/mount.h>
#include <linux/vfs.h>
#include <linux/parser.h>
#include <linux/uio.h>
#include <linux/writeback.h>
#include <linux/log2.h>
#include <linux/hash.h>
#include <asm/unaligned.h>

#include <linux/blkdev.h>


#include "sfat.h"
#include "super.h"
#include "io.h"
#include "inode.h"

static const struct super_operations sfat_sops = {
    // callback for allocating memory for inode
    .alloc_inode    = sfat_alloc_inode,

    // callback for releasing memory for inode
    .destroy_inode  = sfat_destroy_inode,


//    .write_inode    = sfat_write_inode,

    // callback when the inode needs to be deleted.
    // (User deletes the file.)
    // Must call clear_inode in the end of this function
    .delete_inode   = sfat_delete_inode,
//    .put_super      = sfat_put_super,
//    .write_super    = sfat_write_super,
//    .sync_fs        = sfat_sync_fs,
//    .statfs         = sfat_statfs,

    // callback before destroy_inode
    .clear_inode    = sfat_clear_inode,
//    .remount_fs     = sfat_remount,
    
//    .show_options   = sfat_show_options,
};


static int parse_options(char *options, int silent,
             struct sfat_mount_options *opts)
{
    opts->fs_uid = current_uid();
    opts->fs_gid = current_gid();
    opts->fs_fmask = opts->fs_dmask = current_umask();

    return 0;
}

/*
 * Read the super block of an SFAT file system
 * silent: whether to use printk to generate message
 * return: 0 is success
 *         < 0 is error code
 *         -EINVAL and etc.
 */
int sfat_fill_super_impl(struct super_block *sb, void *data, int silent)
{
    int error = 0;
    char buf[BDEVNAME_SIZE];

    struct block_device *bdev = NULL;
    struct inode *bd_inode = NULL;

    unsigned short blksz_bdev_logic = 0;
    unsigned blksz_bdev = 0;
    unsigned long blksz_super = 0;
    unsigned int blkbits_inode = 0;
    loff_t size_inode = 0;  // size (in bytes of the whole block device)

    int minsize = 0;  // block size

    struct block_holder *bh = NULL;
    struct sfat_boot_sector *bs = NULL;

    struct sfat_sb_info *sbi = NULL;
    struct sfat_fs_info *fs_info = NULL;
    unsigned int media = 0;

    u16 sector_size = 0;
    u16 reserved = 0;

    struct inode *root_inode = 0;

    char *p = NULL;
    // end of local variables definition

    minsize = bdev_logical_block_size(sb->s_bdev);
    sb_set_blocksize(sb, minsize);

    bdev = sb->s_bdev;
    memset(buf, 0, BDEVNAME_SIZE);
    bdevname(bdev, buf);
    buf[BDEVNAME_SIZE-1] = '\0';
    printk (KERN_INFO "sfat: sfat_fill_super_impl: device name is %s\n", buf);

    bd_inode = bdev->bd_inode;
    if (!bd_inode)
    {
        printk (KERN_INFO "sfat: sfat_fill_super_impl: bd_inode is null\n");
        return -EINVAL;
    }

    blksz_bdev_logic = bdev_logical_block_size(bdev);
    blksz_bdev = bdev->bd_block_size;
    blksz_super = sb->s_blocksize;
    blkbits_inode = bd_inode->i_blkbits;
    size_inode = bd_inode->i_size;

    // just for fun
    printk (KERN_INFO "sfat: sfat_fill_super_impl: blksz_bdev_logic = %u, "
            "blksz_bdev = %u, blksz_super = %lu, blkbits_inode = %u, size_inode = %lld\n",
            (unsigned int)blksz_bdev_logic, blksz_bdev, blksz_super, blkbits_inode, size_inode);

    if (data)
    {
        while ((p = strsep((char**)&data, ",")) != NULL)
        {
            if (!*p)
            {
                continue;
            }
            printk (KERN_INFO "sfat: sfat_fill_super_impl: option is %s\n", p);
        }
    }

    /*
     * GFP_KERNEL is ok here, because while we do hold the
     * supeblock lock, memory pressure can't call back into
     * the filesystem, since we're only just about to mount
     * it and have no inodes etc active!
     */
    // create super_block for the currently mounted fs
    sbi = kzalloc(sizeof(struct sfat_sb_info), GFP_KERNEL);
    if (!sbi)
    	return -ENOMEM;

    fs_info = &sbi->fs_info;
    fs_info->block_size = blksz_bdev_logic;
    fs_info->block_bits = ffs(fs_info->block_size) - 1;  // if size = 4, then bits = 2

    sb->s_fs_info = sbi;
    
    sb->s_flags |= MS_NODIRATIME;
    sb->s_magic = SFAT_MEDIA;  // original is MSDOS_SUPER_MAGIC;
    sb->s_op = &sfat_sops;  // callback for matipulating meta-info of the whole file system as well as inodes
    sb->s_export_op = 0;  // &fat_export_ops;  // todo: Does 0 suffice?

    error = parse_options(data, silent, &sbi->options);
    if (error)
        goto out_release_sbi;

    bh = sfat_blkholder_alloc();
    if (!bh)
    {
        printk (KERN_INFO "sfat: sfat_fill_super_impl() alloc block_holder failed\n");
        error = -ENOMEM;
        goto out_release_sbi;
    }
    
    error = read_block(bdev, bh, sbi->fs_info.block_size, 0);
    if (error)
    {
        goto out_release_bh;
    }

    bs = (struct sfat_boot_sector *)sfat_blkholder_get_data(bh);
    printk (KERN_INFO "sfat: sfat_fill_super_impl() media is 0x%x\n", bs->media);

    media = bs->media;
    if (SFAT_MEDIA != media)
    {
        if (!silent)
        {
            printk(KERN_ERR "SFAT: invalid media value (0x%02x)\n", media);
        }
        error = -EINVAL;
    	goto out_release_bh;
    }
    sector_size = le16_to_cpu(bs->sector_size);
    if (!is_power_of_2(sector_size)
        || (sector_size < 512)
        || (sector_size > 4096))
    {
    	if (!silent)
        {
            printk(KERN_ERR "SFAT: bogus logical sector size %u\n", sector_size);
        }
        error = -EINVAL;
    	goto out_release_bh;
    }

    // get the basic fixed info of the file system
    fs_info->sector_size = sector_size;
    fs_info->sector_bits = ffs(fs_info->sector_size) - 1;  // if size is 8, then bits is 3

    fs_info->sec_per_clus = bs->sec_per_clus;

    fs_info->cluster_size = fs_info->sec_per_clus << (fs_info->sector_bits);
    fs_info->cluster_bits = ffs(fs_info->cluster_size) - 1;

    fs_info->blk_per_sec_bits = fs_info->sector_bits - fs_info->block_bits;
    fs_info->blk_per_sec = 1 << fs_info->blk_per_sec_bits;

    fs_info->blk_per_clus_bits = fs_info->cluster_bits - fs_info->block_bits;
    fs_info->blk_per_clus = 1 << fs_info->blk_per_clus_bits;

    fs_info->dirent_per_blk = fs_info->block_size >> 5;  // one entry is 32 bytes



#define SEC_2_BLK(SEC) ((SEC) << (fs_info->blk_per_sec_bits))

    reserved = le16_to_cpu(bs->reserved);
    fs_info->fat_start_sec = 1 + reserved;
    fs_info->fat_start_blk = SEC_2_BLK(fs_info->fat_start_sec);

    fs_info->fat_length_sec = le32_to_cpu(bs->fat_length);
    fs_info->fat_length_blk = SEC_2_BLK(fs_info->fat_length_sec);

    fs_info->fats = bs->fats;

    fs_info->data_start_sec = fs_info->fat_start_sec + fs_info->fat_length_sec * fs_info->fats;
    fs_info->data_start_blk = SEC_2_BLK(fs_info->data_start_sec);


    fs_info->clusters = le32_to_cpu(bs->clusters);

    fs_info->root_cluster_cls = le32_to_cpu(bs->root_start);

    sbi->root_size = le32_to_cpu(bs->root_size);

    // end of initialization of sbi


    root_inode = new_inode(sb);
    if (!root_inode)
    {
        error = -ENOMEM;
    	goto out_release_bh;
    }

    // SFAT_ROOT_INO is a number I pick casually for root
    root_inode->i_ino = SFAT_ROOT_INO;
    root_inode->i_version = 1;
    error = sfat_read_root(root_inode);
    if (error < 0)
    {   
        printk(KERN_INFO "SFAT: sfat_read_root failed, error is %d\n", error);
    	goto out_release_root;
    }
    printk(KERN_INFO "SFAT: sfat_read_root 0030\n");
    insert_inode_hash(root_inode);
    printk(KERN_INFO "SFAT: sfat_read_root 0040\n");
    sb->s_root = d_alloc_root(root_inode);
    printk(KERN_INFO "SFAT: sfat_read_root 0050\n");
    if (!sb->s_root) 
    {
        error = -ENOMEM;
    	printk(KERN_INFO "SFAT: get root inode dentry failed\n");
    	goto out_release_root;
    }

    // finally we succeed
    sfat_blkholder_free(bh);
    printk(KERN_INFO "SFAT: sfat_fill_super_impl success\n");
    return 0;

out_release_root:
    iput(root_inode);

out_release_bh:
//    if (!silent)
//    {
        printk(KERN_INFO "VFS: Can't find a valid SFAT filesystem"
		       " on dev %s.\n", sb->s_id);
//    }
    sfat_blkholder_free(bh);

out_release_sbi:
    printk(KERN_INFO "SFAT: sfat_fill_super_impl out_release_sbi\n");
    sb->s_fs_info = NULL;
    kfree(sbi);
    return error;
}



