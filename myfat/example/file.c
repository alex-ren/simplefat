/*
 *  linux/fs/fat/file.c
 *
 *  Written 1992,1993 by Werner Almesberger
 *
 *  regular file handling primitives for fat-based filesystems
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
#include "fat.h"
#include "myfat_fs.h"

static int fat_ioctl_get_attributes(struct inode *inode, u32 __user *user_attr)
{
	u32 attr;

	mutex_lock(&inode->i_mutex);
	attr = fat_make_attrs(inode);
	mutex_unlock(&inode->i_mutex);

	return put_user(attr, user_attr);
}

static int fat_ioctl_set_attributes(struct file *file, u32 __user *user_attr)
{
	struct inode *inode = file->f_path.dentry->d_inode;
	struct msdos_sb_info *sbi = MSDOS_SB(inode->i_sb);
	int is_dir = S_ISDIR(inode->i_mode);
	u32 attr, oldattr;
	struct iattr ia;
	int err;

	err = get_user(attr, user_attr);
	if (err)
		goto out;

	mutex_lock(&inode->i_mutex);
	err = mnt_want_write(file->f_path.mnt);
	if (err)
		goto out_unlock_inode;

	/*
	 * ATTR_VOLUME and ATTR_DIR cannot be changed; this also
	 * prevents the user from turning us into a VFAT
	 * longname entry.  Also, we obviously can't set
	 * any of the NTFS attributes in the high 24 bits.
	 */
	attr &= 0xff & ~(ATTR_VOLUME | ATTR_DIR);
	/* Merge in ATTR_VOLUME and ATTR_DIR */
	attr |= (MSDOS_I(inode)->i_attrs & ATTR_VOLUME) |
		(is_dir ? ATTR_DIR : 0);
	oldattr = fat_make_attrs(inode);

	/* Equivalent to a chmod() */
	ia.ia_valid = ATTR_MODE | ATTR_CTIME;
	ia.ia_ctime = current_fs_time(inode->i_sb);
	if (is_dir)
		ia.ia_mode = fat_make_mode(sbi, attr, S_IRWXUGO);
	else {
		ia.ia_mode = fat_make_mode(sbi, attr,
			S_IRUGO | S_IWUGO | (inode->i_mode & S_IXUGO));
	}

	/* The root directory has no attributes */
	if (inode->i_ino == MSDOS_ROOT_INO && attr != ATTR_DIR) {
		err = -EINVAL;
		goto out_drop_write;
	}

	if (sbi->options.sys_immutable &&
	    ((attr | oldattr) & ATTR_SYS) &&
	    !capable(CAP_LINUX_IMMUTABLE)) {
		err = -EPERM;
		goto out_drop_write;
	}

	/*
	 * The security check is questionable...  We single
	 * out the RO attribute for checking by the security
	 * module, just because it maps to a file mode.
	 */
	err = security_inode_setattr(file->f_path.dentry, &ia);
	if (err)
		goto out_drop_write;

	/* This MUST be done before doing anything irreversible... */
	err = fat_setattr(file->f_path.dentry, &ia);
	if (err)
		goto out_drop_write;

	fsnotify_change(file->f_path.dentry, ia.ia_valid);
	if (sbi->options.sys_immutable) {
		if (attr & ATTR_SYS)
			inode->i_flags |= S_IMMUTABLE;
		else
			inode->i_flags &= S_IMMUTABLE;
	}

	fat_save_attrs(inode, attr);
	mark_inode_dirty(inode);
out_drop_write:
	mnt_drop_write(file->f_path.mnt);
out_unlock_inode:
	mutex_unlock(&inode->i_mutex);
out:
	return err;
}


static int fat_ioctl_myfat_test(struct inode *inode, struct file *filp)
{
	struct super_block *sb = inode->i_sb;
	struct block_device *bdev = 0;
	struct inode *bd_inode = 0;

	unsigned short logic_blk_sz1 = 0;
	unsigned logic_blk_sz2 = 0;
	unsigned int blkbits = 0;
	loff_t size = 0;  // size (in bytes of the whole block device)

	gfp_t gfp_mask = 0;
	struct page *page = 0;

	struct buffer_head *bh = 0;

	if (!sb)
	{
		printk (KERN_INFO "myfat: fat_ioctl_myfat_test: sb is null\n");
		return 0;
	}


	bdev = sb->s_bdev;
	if (!bdev)
	{
		printk (KERN_INFO "myfat: fat_ioctl_myfat_test: bdev is null\n");
		return 0;
	}

	bd_inode = bdev->bd_inode;
	if (!bd_inode)
	{
		printk (KERN_INFO "myfat: fat_ioctl_myfat_test: bd_inode is null\n");
		return 0;
	}

	logic_blk_sz1 = bdev_logical_block_size(bdev);
	logic_blk_sz2 = bdev->bd_block_size;
	blkbits = bd_inode->i_blkbits;
	size = bd_inode->i_size;

	gfp_mask = (mapping_gfp_mask(inode->i_mapping) & ~__GFP_FS)|__GFP_MOVABLE;

	page = __page_cache_alloc(gfp_mask);

	if (!page)
	{
		printk (KERN_INFO "myfat: fat_ioctl_myfat_test: __page_cache_alloc failed\n");
		return 0;
	}

	lock_page(page);  // lock the page

	bh = alloc_buffer_head(GFP_NOFS);
	if (!bh)
	{
		printk (KERN_INFO "myfat: fat_ioctl_myfat_test: alloc_buffer_head failed\n");
		goto failed;
	}

	// rzq: quoted from alloc_page_buffers()
	bh->b_bdev = NULL;
	bh->b_this_page = NULL;
	bh->b_blocknr = -1;
	bh->b_state = 0;
	atomic_set(&bh->b_count, 0);
	bh->b_private = NULL;
	bh->b_size = logic_blk_sz2;

	/* Link the buffer to its page */
	set_bh_page(bh, page, 0);
	init_buffer(bh, NULL, NULL);

	bh->b_bdev = bdev;
	bh->b_blocknr = 30000;  // rzq: A number I pick randomly.
	// set_buffer_uptodate(bh);
	set_buffer_mapped(bh);

	// rzq: prepare for end_buffer_read_sync() (see below)
	lock_buffer(bh);  // lock the buffer head, end_buffer_read_sync (see below) contains unlock
	get_bh(bh);  // end_buffer_read_sync (see below) contains put_bh
	clear_buffer_uptodate(bh);  // clear the bit for future test of whether the read succeeds

	bh->b_end_io = end_buffer_read_sync;

	submit_bh(READ, bh);
	wait_on_buffer(bh);

	if (buffer_uptodate(bh))
	{
		printk (KERN_INFO "myfat: fat_ioctl_myfat_test: bh is updated\n");
		printk (KERN_INFO "myfat: fat_ioctl_myfat_test: bh->b_data[0] is %d\n", (int)(bh->b_data[0]));

	}
	else
	{
		printk (KERN_INFO "myfat: fat_ioctl_myfat_test: bh is not updated\n");
		goto bh_failed;
	}

	bh->b_data[0] = bh->b_data[0] + 1;

	lock_buffer(bh);  // lock the buffer head, end_buffer_write_sync (see below) contains unlock
	get_bh(bh);  // end_buffer_read_sync (see below) contains put_bh
	clear_buffer_uptodate(bh);  // clear the bit for future test of whether the read succeeds

	bh->b_end_io = end_buffer_write_sync;
	submit_bh(WRITE_BARRIER, bh);
	wait_on_buffer(bh);

	if (buffer_uptodate(bh))
	{
		printk (KERN_INFO "myfat: fat_ioctl_myfat_test: write bh is updated\n");
	}
	else
	{
		printk (KERN_INFO "myfat: fat_ioctl_myfat_test: write bh is not updated\n");
		goto bh_failed;
	}

bh_failed:
	free_buffer_head(bh);
failed:
	unlock_page(page);
	page_cache_release(page);

	printk (KERN_INFO "myfat: fat_ioctl_myfat_test: sz1 = %u, sz2 = %u, blkbits = %u, size of block device is %lld\n",
			(unsigned int)logic_blk_sz1, logic_blk_sz2, blkbits, size);





	return 0;
}

int fat_generic_ioctl(struct inode *inode, struct file *filp,
		      unsigned int cmd, unsigned long arg)
{
	u32 __user *user_attr = (u32 __user *)arg;

	printk (KERN_INFO "myfat: fat_generic_ioctl\n");

	switch (cmd) {
	case FAT_IOCTL_GET_ATTRIBUTES:
		return fat_ioctl_get_attributes(inode, user_attr);
	case FAT_IOCTL_SET_ATTRIBUTES:
		return fat_ioctl_set_attributes(filp, user_attr);
	case FAT_IOCTL_MYFAT_TEST:
	{
		printk (KERN_INFO "myfat: FAT_IOCTL_MYFAT_TEST\n");
		fat_ioctl_myfat_test(inode, filp);
		return 0;
	}
	default:
		return -ENOTTY;	/* Inappropriate ioctl for device */
	}
}

static int fat_file_release(struct inode *inode, struct file *filp)
{
	printk (KERN_INFO "myfat: fat_file_release\n");
	if ((filp->f_mode & FMODE_WRITE) &&
	     MSDOS_SB(inode->i_sb)->options.flush) {
		fat_flush_inodes(inode->i_sb, inode, NULL);
		congestion_wait(BLK_RW_ASYNC, HZ/10);
	}
	return 0;
}

int fat_file_fsync(struct file *filp, struct dentry *dentry, int datasync)
{
	struct inode *inode = dentry->d_inode;
	int res, err;

	printk (KERN_INFO "myfat: fat_file_fsync\n");

	res = simple_fsync(filp, dentry, datasync);
	err = sync_mapping_buffers(MSDOS_SB(inode->i_sb)->fat_inode->i_mapping);

	return res ? res : err;
}


const struct file_operations fat_file_operations = {
	.llseek		= generic_file_llseek,
	.read		= do_sync_read,
	.write		= do_sync_write,
	.aio_read	= generic_file_aio_read,
	.aio_write	= generic_file_aio_write,
	.mmap		= generic_file_mmap,
	.release	= fat_file_release,
	.ioctl		= fat_generic_ioctl,
	.fsync		= fat_file_fsync,
	.splice_read	= generic_file_splice_read,
};

static int fat_cont_expand(struct inode *inode, loff_t size)
{
	struct address_space *mapping = inode->i_mapping;
	loff_t start = inode->i_size, count = size - inode->i_size;
	int err;

	err = generic_cont_expand_simple(inode, size);
	if (err)
		goto out;

	inode->i_ctime = inode->i_mtime = CURRENT_TIME_SEC;
	mark_inode_dirty(inode);
	if (IS_SYNC(inode)) {
		int err2;

		/*
		 * Opencode syncing since we don't have a file open to use
		 * standard fsync path.
		 */
		err = filemap_fdatawrite_range(mapping, start,
					       start + count - 1);
		err2 = sync_mapping_buffers(mapping);
		if (!err)
			err = err2;
		err2 = write_inode_now(inode, 1);
		if (!err)
			err = err2;
		if (!err) {
			err =  filemap_fdatawait_range(mapping, start,
						       start + count - 1);
		}
	}
out:
	return err;
}

/* Free all clusters after the skip'th cluster. */
static int fat_free(struct inode *inode, int skip)
{
	struct super_block *sb = inode->i_sb;
	int err, wait, free_start, i_start, i_logstart;

	if (MSDOS_I(inode)->i_start == 0)
		return 0;

	fat_cache_inval_inode(inode);

	wait = IS_DIRSYNC(inode);
	i_start = free_start = MSDOS_I(inode)->i_start;
	i_logstart = MSDOS_I(inode)->i_logstart;

	/* First, we write the new file size. */
	if (!skip) {
		MSDOS_I(inode)->i_start = 0;
		MSDOS_I(inode)->i_logstart = 0;
	}
	MSDOS_I(inode)->i_attrs |= ATTR_ARCH;
	inode->i_ctime = inode->i_mtime = CURRENT_TIME_SEC;
	if (wait) {
		err = fat_sync_inode(inode);
		if (err) {
			MSDOS_I(inode)->i_start = i_start;
			MSDOS_I(inode)->i_logstart = i_logstart;
			return err;
		}
	} else
		mark_inode_dirty(inode);

	/* Write a new EOF, and get the remaining cluster chain for freeing. */
	if (skip) {
		struct fat_entry fatent;
		int ret, fclus, dclus;

		ret = fat_get_cluster(inode, skip - 1, &fclus, &dclus);
		if (ret < 0)
			return ret;
		else if (ret == FAT_ENT_EOF)
			return 0;

		fatent_init(&fatent);
		ret = fat_ent_read(inode, &fatent, dclus);
		if (ret == FAT_ENT_EOF) {
			fatent_brelse(&fatent);
			return 0;
		} else if (ret == FAT_ENT_FREE) {
			fat_fs_error(sb,
				     "%s: invalid cluster chain (i_pos %lld)",
				     __func__, MSDOS_I(inode)->i_pos);
			ret = -EIO;
		} else if (ret > 0) {
			err = fat_ent_write(inode, &fatent, FAT_ENT_EOF, wait);
			if (err)
				ret = err;
		}
		fatent_brelse(&fatent);
		if (ret < 0)
			return ret;

		free_start = ret;
	}
	inode->i_blocks = skip << (MSDOS_SB(sb)->cluster_bits - 9);

	/* Freeing the remained cluster chain */
	return fat_free_clusters(inode, free_start);
}

void fat_truncate(struct inode *inode)
{
	struct msdos_sb_info *sbi = MSDOS_SB(inode->i_sb);
	const unsigned int cluster_size = sbi->cluster_size;
	int nr_clusters;
	printk (KERN_INFO "myfat: fat_truncate\n");

	/*
	 * This protects against truncating a file bigger than it was then
	 * trying to write into the hole.
	 */
	if (MSDOS_I(inode)->mmu_private > inode->i_size)
		MSDOS_I(inode)->mmu_private = inode->i_size;

	nr_clusters = (inode->i_size + (cluster_size - 1)) >> sbi->cluster_bits;

	fat_free(inode, nr_clusters);
	fat_flush_inodes(inode->i_sb, inode, NULL);
}

int fat_getattr(struct vfsmount *mnt, struct dentry *dentry, struct kstat *stat)
{
	struct inode *inode = dentry->d_inode;

	printk (KERN_INFO "myfat: fat_getattr mode is %d\n", inode->i_mode);

	generic_fillattr(inode, stat);
	stat->blksize = MSDOS_SB(inode->i_sb)->cluster_size;
    printk (KERN_INFO "myfat: fat_getattr \n"
            "dev = %u\n"
            "ino = %llu\n"
            "mode = %u\n"
            "nlink = %u\n"
            "uid = %u\n"
            "gid = %u\n"
            "rdev = %u\n"
//            "atime = %u\n"
//            "mtime = %u\n"
//            "ctime = %u\n"
            "size = %llu\n"
            "blocks = %llu\n"
            "blksize = %lu\n",
            stat->dev,
            stat->ino,
            stat->mode,
            stat->nlink,
            stat->uid,
            stat->gid,
            stat->rdev,
//            stat->atime,
//            stat->mtime,
//            stat->ctime,
            stat->size,
            stat->blocks,
            stat->blksize);
	return 0;
}
EXPORT_SYMBOL_GPL(fat_getattr);

static int fat_sanitize_mode(const struct msdos_sb_info *sbi,
			     struct inode *inode, umode_t *mode_ptr)
{
	mode_t mask, perm;

	/*
	 * Note, the basic check is already done by a caller of
	 * (attr->ia_mode & ~FAT_VALID_MODE)
	 */

	if (S_ISREG(inode->i_mode))
		mask = sbi->options.fs_fmask;
	else
		mask = sbi->options.fs_dmask;

	perm = *mode_ptr & ~(S_IFMT | mask);

	/*
	 * Of the r and x bits, all (subject to umask) must be present. Of the
	 * w bits, either all (subject to umask) or none must be present.
	 *
	 * If fat_mode_can_hold_ro(inode) is false, can't change w bits.
	 */
	if ((perm & (S_IRUGO | S_IXUGO)) != (inode->i_mode & (S_IRUGO|S_IXUGO)))
		return -EPERM;
	if (fat_mode_can_hold_ro(inode)) {
		if ((perm & S_IWUGO) && ((perm & S_IWUGO) != (S_IWUGO & ~mask)))
			return -EPERM;
	} else {
		if ((perm & S_IWUGO) != (S_IWUGO & ~mask))
			return -EPERM;
	}

	*mode_ptr &= S_IFMT | perm;

	return 0;
}

static int fat_allow_set_time(struct msdos_sb_info *sbi, struct inode *inode)
{
	mode_t allow_utime = sbi->options.allow_utime;

	if (current_fsuid() != inode->i_uid) {
		if (in_group_p(inode->i_gid))
			allow_utime >>= 3;
		if (allow_utime & MAY_WRITE)
			return 1;
	}

	/* use a default check */
	return 0;
}

#define TIMES_SET_FLAGS	(ATTR_MTIME_SET | ATTR_ATIME_SET | ATTR_TIMES_SET)
/* valid file mode bits */
#define FAT_VALID_MODE	(S_IFREG | S_IFDIR | S_IRWXUGO)

int fat_setattr(struct dentry *dentry, struct iattr *attr)
{
	struct msdos_sb_info *sbi = MSDOS_SB(dentry->d_sb);
	struct inode *inode = dentry->d_inode;
	unsigned int ia_valid;
	int error;

	printk (KERN_INFO "myfat: fat_setattr\n");

	/*
	 * Expand the file. Since inode_setattr() updates ->i_size
	 * before calling the ->truncate(), but FAT needs to fill the
	 * hole before it.
	 */
	if (attr->ia_valid & ATTR_SIZE) {
		if (attr->ia_size > inode->i_size) {
			error = fat_cont_expand(inode, attr->ia_size);
			if (error || attr->ia_valid == ATTR_SIZE)
				goto out;
			attr->ia_valid &= ~ATTR_SIZE;
		}
	}

	/* Check for setting the inode time. */
	ia_valid = attr->ia_valid;
	if (ia_valid & TIMES_SET_FLAGS) {
		if (fat_allow_set_time(sbi, inode))
			attr->ia_valid &= ~TIMES_SET_FLAGS;
	}

	error = inode_change_ok(inode, attr);
	attr->ia_valid = ia_valid;
	if (error) {
		if (sbi->options.quiet)
			error = 0;
		goto out;
	}

	if (((attr->ia_valid & ATTR_UID) &&
	     (attr->ia_uid != sbi->options.fs_uid)) ||
	    ((attr->ia_valid & ATTR_GID) &&
	     (attr->ia_gid != sbi->options.fs_gid)) ||
	    ((attr->ia_valid & ATTR_MODE) &&
	     (attr->ia_mode & ~FAT_VALID_MODE)))
		error = -EPERM;

	if (error) {
		if (sbi->options.quiet)
			error = 0;
		goto out;
	}

	/*
	 * We don't return -EPERM here. Yes, strange, but this is too
	 * old behavior.
	 */
	if (attr->ia_valid & ATTR_MODE) {
		if (fat_sanitize_mode(sbi, inode, &attr->ia_mode) < 0)
			attr->ia_valid &= ~ATTR_MODE;
	}

	if (attr->ia_valid)
		error = inode_setattr(inode, attr);
out:
	return error;
}
EXPORT_SYMBOL_GPL(fat_setattr);

const struct inode_operations fat_file_inode_operations = {
	.truncate	= fat_truncate,
	.setattr	= fat_setattr,
	.getattr	= fat_getattr,
};
