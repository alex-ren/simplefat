/*
 *  linux/fs/myfat/simplefat/namei.c
 *
 *  Written 2012 by Zhiqiang Ren
 */

#include <linux/module.h>
#include <linux/time.h>
#include <linux/buffer_head.h>

#include "super.h"
#include "io.h"
#include "inode.h"

static int sfat_fill_super(struct super_block *sb, void *data, int silent)
{
	int res;

	res = sfat_fill_super_impl(sb, data, silent);
	if (res)
		return res;

	sb->s_flags |= MS_NOATIME;
	sb->s_root->d_op = 0;  // todo &msdos_dentry_operations;
	return 0;
}

static int sfat_get_sb(struct file_system_type *fs_type,
			int flags, const char *dev_name,
			void *data, struct vfsmount *mnt)
{
	// rzq: get_sb_bdev() is a library function
	return get_sb_bdev(fs_type, flags, dev_name, data, sfat_fill_super,
			   mnt);
}

static struct file_system_type sfat_fs_type = {
    // fixed
	.owner		= THIS_MODULE,

	// the name of your file system, the same as the name we use when doing mount.
	// e.g. mount -t simplefat /dev/loop1 ./testbed
	.name		= "simplefat",

	// a callback function provided by us
	.get_sb		= sfat_get_sb,

	// a callback function provided by us,
	// actuall we just use the Linux library function kill_block_super
	.kill_sb	= kill_block_super,

	// some flag we choose
	.fs_flags	= FS_REQUIRES_DEV,
};

static int __init init_sfat_fs(void)
{
    int ret = sfat_blkholder_cache_init();
    if (ret)
    {
    	printk (KERN_INFO "sfat_blkholder_cache_init failed\n");
        return ret;
    }
    ret = sfat_inodeinfo_cache_init();
    if (ret)
    {
    	printk (KERN_INFO "sfat_inodeinfo_cache_init failed\n");
        return ret;
    }

    return register_filesystem(&sfat_fs_type);
}

static void __exit exit_sfat_fs(void)
{
    sfat_blkholder_cache_destroy();
    sfat_inodeinfo_cache_destroy();
    unregister_filesystem(&sfat_fs_type);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zhiqiang Ren");
MODULE_DESCRIPTION("Simple FAT filesystem support");

module_init(init_sfat_fs)
module_exit(exit_sfat_fs)



