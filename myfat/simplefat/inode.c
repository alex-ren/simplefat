#include "inode.h"
#include "sfat.h"
#include "io.h"



static struct kmem_cache *sfat_cache_inodeinfo = 0;

/* ********** ********** ************* */
/* initialize the cache for sfat_inode_info
 *
 */

/*
 * Desc: initialization of sfat_inode_info
 */
static void init_once(void *foo) {
    struct sfat_inode_info *ei = (struct sfat_inode_info *) foo;
    ei->i_start = 0;
    ei->i_attrs = 0;
    ei->i_pos = 0;

    inode_init_once(&ei->vfs_inode);
}

/*
 * return: 0 => success
 *         -ENOMEM
 */
int __init sfat_inodeinfo_cache_init(void) {
    sfat_cache_inodeinfo = kmem_cache_create("sfat_inodeinfo_cache",
            sizeof(struct sfat_inode_info), 0,
            (SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD), init_once);
    if (sfat_cache_inodeinfo == NULL) {
        return -ENOMEM;
    }

    return 0;
}

void sfat_inodeinfo_cache_destroy(void) {
    kmem_cache_destroy(sfat_cache_inodeinfo);
    sfat_cache_inodeinfo = 0;
}


/* ******** ********** ************ */
int sfat_create_file(struct inode *dir, struct dentry *dentry, int mode,
            struct nameidata *nd);

struct dentry * sfat_lookup(struct inode *dir,struct dentry *dentry, struct nameidata *data);

int sfat_setattr(struct dentry *de, struct iattr *attr);

int sfat_getattr(struct vfsmount *mnt, struct dentry *dentry, struct kstat *stat);

int sfat_readdir(struct file *filp, void *dirent, filldir_t filldir);

ssize_t sfat_sync_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos);

ssize_t sfat_sync_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos);

// operations for a directory
static const struct inode_operations sfat_dir_inode_operations = {
        .create = sfat_create_file,
        .lookup = sfat_lookup,
        .unlink = 0, // msdos_unlink,
        .mkdir = 0, // msdos_mkdir,
        .rmdir = 0, // msdos_rmdir,
        .rename = 0, // msdos_rename,
        .setattr = sfat_setattr,
        .getattr = sfat_getattr,
        };

// operations for a regular file
const struct file_operations sfat_dir_file_operations = {
    .llseek = generic_file_llseek,
    .read = generic_read_dir,
    .readdir = sfat_readdir,
    .ioctl = 0, // fat_dir_ioctl, todo
    .fsync = 0, // fat_file_fsync, todo
};

// operations for a directory
static const struct inode_operations sfat_file_inode_operations = {
    .truncate   = 0,  // fat_truncate, todo
    .setattr    = 0,  // fat_setattr, todo
    .getattr    = 0,  // fat_getattr, todo
};

static const struct file_operations sfat_file_file_operations = {
    .llseek     = 0,  // generic_file_llseek,  todo
    .read       = sfat_sync_read,  // do_sync_read,
    .write      = sfat_sync_write,  // do_sync_write
    .aio_read   = 0,  // generic_file_aio_read,  todo
    .aio_write  = 0,  // generic_file_aio_write,  todo
    .mmap       = 0,  // generic_file_mmap,  todo
    .release    = 0,  // fat_file_release,  todo
    .ioctl      = 0,  // fat_generic_ioctl,  todo
    .fsync      = 0,  // fat_file_fsync,  todo
    .splice_read = 0,  // generic_file_splice_read,  todo
};


static const struct dentry_operations sfat_dentry_operations = {
    .d_hash     = 0,  // msdos_hash,  todo
    .d_compare  = 0,  // msdos_cmp,  todo
};

/* ********** ********** ************* */

/*
 * count the number of characters till null
 */
static inline size_t str_len(char *str, size_t max)
{
    size_t count = 0;
    for (; count < max; ++count)
    {
        if (str[count] == '\0')
        {
            break;
        }
    }
    return count;
}

int sfat_get_entry_content(struct sfat_fs_info *fs, struct block_device *bdev, size_t cls, size_t *next);


/*
 * Desc: locate a position (report its cluster and offset)
 * cur_cls: starting cluster
 * pos: target position (logic position in file) counted from the beginning of cur_cls,
 *      this can be a very large number
 * cls: output value => no. of cluster
 * offset: output value => offset in the cluster
 * return: 0 is success
 *         < 0 is error code
 */
static inline int sfat_seek(struct sfat_fs_info *fs, struct block_device *bdev,
            size_t cur_cls, loff_t pos,
            size_t *cls, size_t *offset)
{
    int error = 0;

    if (cur_cls >= fs->clusters - 1) {
        return -EINVAL;
    }

    while (pos >= fs->cluster_size)
    {
        error = sfat_get_entry_content(fs, bdev, cur_cls, &cur_cls);
        if (error)
        {
            return error;
        }
        if (cur_cls > fs->clusters)  // stop prematurely
        {
            return -EINVAL;
        }

        pos -= fs->cluster_size;
    }

    *cls = cur_cls;
    *offset = pos;  // type conversion is safe since pos is small
    return 0;
}


// ============================================

// root the info for root directory from disk and store it in inode
// Return Value: < 0 : error code
int sfat_read_root(struct inode *inode) {
    struct super_block *sb = inode->i_sb;
    struct sfat_sb_info *sbi = SFAT_SB(sb);
    struct sfat_inode_info *inodei = SFAT_I(inode);
    int subfiles = 0;

    printk(KERN_INFO "sfat: sfat_read_root\n");

    // This is root. It has no corresponding directory entry in some other directory file.
    inodei->i_pos = SFAT_ROOT_DIRENTRY_POS; // This is a special value.
    inodei->i_start = sbi->fs_info.root_cluster_cls;
    inodei->i_attrs = SFAT_ATTR_DIR;

    inode->i_uid = sbi->options.fs_uid;
    inode->i_gid = sbi->options.fs_gid;
    inode->i_version++; // copied from fat, no reason why
    inode->i_generation = 0;
    inode->i_mode = sfat_make_mode(sbi, SFAT_ATTR_DIR, S_IRWXUGO);
    inode->i_op = &sfat_dir_inode_operations; // callback for manipulating meta-info of file.
    inode->i_fop = &sfat_dir_file_operations; // callback for manipulating content of file.

    // the size of the file for Root Dir in bytes
    inode->i_size = sbi->root_size << sbi->fs_info.cluster_bits;
    printk(KERN_INFO "sfat: sfat_read_root inode->i_size = %lld\n", inode->i_size);

    inode->i_mtime.tv_sec = inode->i_atime.tv_sec = inode->i_ctime.tv_sec = 0;
    inode->i_mtime.tv_nsec = inode->i_atime.tv_nsec = inode->i_ctime.tv_nsec = 0;

    // how many blocks are consumed on the hard disk
    inode->i_blocks = ((inode->i_size + (sbi->fs_info.cluster_size - 1))
               & ~((loff_t)sbi->fs_info.cluster_size - 1)) >> 9;

    subfiles = sfat_count_subdirs(inode);
    if (subfiles < 0) {
        return subfiles;
    }
    inode->i_nlink = subfiles + 2 + 1; // count in . and ..
    printk(KERN_INFO "sfat: sfat_read_root success\n");
    return 0;
}

// count the number of subdirs
// assumption:
//   inode represents a directory
// Return Value:
//   >= 0 valid number
//   < 0 error code
int sfat_count_subdirs(struct inode *inode) {
    struct super_block *sb = inode->i_sb;
    struct block_device *bdev = sb->s_bdev;
    struct sfat_sb_info *sbi = SFAT_SB(sb);
    struct sfat_inode_info *inodei = SFAT_I(inode);
    struct sfat_fs_info *fs_info = &sbi->fs_info;

    int error = 0;
    int count = 0;
    size_t cls = inodei->i_start;
    size_t size = inode->i_size; // type changing from loff_t to size_t

    size_t rounds = 1 + ((size + fs_info->cluster_size - 1) >> fs_info->cluster_bits);

    size_t blk = 0;

    struct block_holder *bh = NULL;

    char *data = NULL;
    struct sfat_dir_entry *de = NULL;

    int i, j = 0;
    // --------------------------
    printk(KERN_INFO "sfat: sfat_count_subdirs\n");

    if (size < 32) {
        return 0; // empty directory
    }

    bh = sfat_blkholder_alloc();
    if (!bh) {
        return -ENOMEM;
    }

    printk(KERN_INFO "sfat: sfat_count_subdirs  001 rounds is %zu\n", rounds);
    while (rounds > 0) {  // just for protection of loop
        --rounds;
        if (cls >= SFAT_ENTRY_MAX) {
            error = -EINVAL;
            printk(KERN_INFO "sfat: sfat_count_subdirs  002\n");
            break;
        }
        printk(KERN_INFO "sfat: sfat_count_subdirs  x0000\n");
        blk = CLS_TO_BLK(fs_info, cls);
        printk(KERN_INFO "sfat: sfat_count_subdirs  x0010\n");
        for (i = 0; i < fs_info->blk_per_clus; ++i) {
            printk(KERN_INFO "sfat: sfat_count_subdirs  003\n");
            error = read_block(bdev, bh, sbi->fs_info.block_size, blk + i);
            if (error) {
                printk(KERN_INFO "sfat: sfat_count_subdirs  004\n");
                goto outloop;
            }

            data = sfat_blkholder_get_data(bh);
            for (j = 0; j < fs_info->block_size; j += 32) {
                printk(KERN_INFO "sfat: sfat_count_subdirs  005\n");
                de = (struct sfat_dir_entry *) (data + j);
                if (de->attr & SFAT_ATTR_EMPTY_END) {
                    printk(KERN_INFO "sfat: sfat_count_subdirs  006\n");
                    goto outloop;
                } else if (de->attr & SFAT_ATTR_EMPTY) {
                    printk(KERN_INFO "sfat: sfat_count_subdirs  007\n");
                    continue;
                } else {
                    printk(KERN_INFO "sfat: sfat_count_subdirs  008\n");
                    ++count;
                }
            }
        }
        printk(KERN_INFO "sfat: sfat_count_subdirs  009\n");
        error = sfat_get_entry_content(fs_info, bdev, cls, &cls);
        // read error or abnormal cluster normal
        if (error || cls > fs_info->clusters) {
            printk(KERN_INFO "sfat: sfat_count_subdirs  020\n");
            break;
        }
    }

outloop:
    printk(KERN_INFO "sfat: sfat_count_subdirs  x0030\n");
    // loop in the fat chain
    if (rounds == 0) {
        error = -EINVAL;
    }

    sfat_blkholder_free(bh);
    if (error) {
        printk(KERN_INFO "sfat: sfat_count_subdirs  031\n");
        return error;
    } else {
        printk(KERN_INFO "sfat: sfat_count_subdirs  032 count is %d\n", count);
        return count;
    }

}

/*
 * next: output value
 * return: 0 is success
 *         < 0 is error code
 */
int sfat_get_entry_content(struct sfat_fs_info *fs, struct block_device *bdev, size_t cls,
        size_t *next) {
    size_t blk = 0; // block
    size_t pos = 0; // entry location in the block in bytes

    struct block_holder *bh = NULL;
    int error = 0;

    if (cls >= fs->clusters - 1) {
        return -EINVAL;
    }

    blk = cls >> (fs->block_bits - 2); // one fat entry need 4 bytes
    pos = (cls - (blk << (fs->block_bits - 2))) << 2;

    blk = blk + fs->fat_start_blk;

    bh = sfat_blkholder_alloc();
    if (!bh) {
        return -ENOMEM;
    }

    error = read_block(bdev, bh, fs->block_size, blk);
    if (error) {
        sfat_blkholder_free(bh);
        return error;
    }

    *next = le32_to_cpu((__le32 *)(sfat_blkholder_get_data(bh) + pos));

    sfat_blkholder_free(bh);
    return 0;
}

/*
 * Scans a directory for a given file
 * Input:
 *   fs:
 *   bdev:
 *   start_cls: start cluster to be searched
 *   name: name of the file, length is at least SFAT_NAME_LEN (padded by \0)
 * Output:
 *   de: directory entry if found
 *   cls:
 *   blk:
 *   offset:
 * Return:
 *   0: success
 *   < 0: error code
 *
 */
int sfat_dentry_locate(struct sfat_fs_info *fs, struct block_device *bdev,
        size_t start_cls, const unsigned char *name,
         struct sfat_dir_entry *de, size_t *cls, size_t *blk, size_t *offset)
{
    size_t i, j, k = 0;
    size_t cur_cls = start_cls;
    size_t cur_blk = 0;
    struct sfat_dir_entry *ent = NULL;

    struct block_holder *bh = NULL;
    int found = 0;
    int error = 0;

    bh = sfat_blkholder_alloc();
    if (!bh) {
        return -ENOMEM;
    }

    for (i = 0; i < fs->clusters; ++i)  // just for protection
    {
        cur_blk = CLS_TO_BLK(fs, cur_cls);

        for (j = 0; j < fs->blk_per_clus; ++j)
        {

            // read block
            error = read_block(bdev, bh, fs->block_size, cur_blk++);
            if (error) {
                goto outloop;
            }

            ent = (struct sfat_dir_entry *)sfat_blkholder_get_data(bh);

            for (k = 0; k < fs->dirent_per_blk; ++k)
            {
                if (ent[k].attr & SFAT_ATTR_EMPTY_END)
                {
                    goto outloop;
                }
                else if (ent[k].attr & SFAT_ATTR_EMPTY)
                {
                    continue;
                }
                else
                {
                    // check entry
                    if(!strncmp(name, ent[k].name, SFAT_NAME_LEN))
                    {
                        memcpy(de, &ent[k], sizeof(struct sfat_dir_entry));
                        *cls = cur_cls;
                        *blk = cur_blk;
                        *offset = k * sizeof(struct sfat_dir_entry);
                        found = 1;
                        goto outloop;
                    }
                }
            }
        }

        error = sfat_get_entry_content(fs, bdev, cur_cls, &cur_cls);
        if (error || cur_cls > fs->clusters)
        {
            goto outloop;
        }
    }

outloop:
    sfat_blkholder_free(bh);

    if (error)
    {
        return error;
    }
    else if (!found)
    {
        return -ENOENT;
    }
    else
    {
        return 0;
    }
}


/*
 * Scans a directory for a free entry
 * Input:
 *   fs:
 *   bdev:
 *   bh: block_holder: holding the data for the whole block
 *   start_cls: start cluster to be searched
 * Output:
 *   cls:
 *   blk:
 *   offset:
 * Return:
 *   0: success
 *   < 0: error code
 *
 */
int sfat_free_dentry_locate(struct sfat_fs_info *fs, struct block_device *bdev,
        struct block_holder *bh,
        size_t start_cls, size_t *cls, size_t *blk, size_t *offset)
{
    size_t i, j, k = 0;
    size_t cur_cls = start_cls;
    size_t cur_blk = 0;
    struct sfat_dir_entry *ent = NULL;

    int found = 0;
    int error = 0;

    for (i = 0; i < fs->clusters; ++i)  // just for protection
    {
        cur_blk = CLS_TO_BLK(fs, cur_cls);

        for (j = 0; j < fs->blk_per_clus; ++j)
        {
            // read block
            error = read_block(bdev, bh, fs->block_size, cur_blk);
            if (error) {
                goto outloop;
            }

            ent = (struct sfat_dir_entry *)sfat_blkholder_get_data(bh);

            // search the entries in one block
            for (k = 0; k < fs->dirent_per_blk; ++k)
            {
                if ((ent[k].attr & SFAT_ATTR_EMPTY_END) ||
                    (ent[k].attr & SFAT_ATTR_EMPTY))
                {
                    *cls = cur_cls;
                    *blk = cur_blk - CLS_TO_BLK(fs, cur_cls);
                    *offset = k * sizeof(struct sfat_dir_entry);
                    found = 1;
                    goto outloop;
                }
                else
                {
                    continue;
                }
            }
            ++cur_blk;
        }

        error = sfat_get_entry_content(fs, bdev, cur_cls, &cur_cls);
        if (error || cur_cls > (fs->clusters)) {
            goto outloop;
        }
    }

outloop:
    if (found)
    {
        return 0;
    }
    else if (error)
    {
        return error;
    }
    else
    {
        return -ENOENT;
    }
}

/*
 * Form the dir_entry base on the information
 * Input:
 *   isdir:
 *   name: string which may be not ended by '\0' if the length is SFAT_NAME_LEN
 *   start_cls:
 *   size: size of the file. If size is 0 then start_cls is useless. start_cluster 
 *         would be SFAT_ENTRY_FREE
 *   ts: time
 * Output:
 *   de:
 *
 */
void sfat_form_dir_entry(struct sfat_dir_entry *de, int isdir,
        const unsigned char *name, int start_cls, size_t size, struct timespec *ts)
{
    int i = 0;

    for (i = 0; i < SFAT_NAME_LEN; ++i)
    {
        de->name[i] = name[i];
        if ('\0' == name[i])
        {
            break;
        }
    }

    for (; i < SFAT_NAME_LEN; ++i)
    {
        de->name[i] = '\0';
    }

    de->attr = isdir? SFAT_ATTR_DIR: SFAT_ATTR_NONE;
    de->size = cpu_to_le32(size);
    printk(KERN_INFO "sfat: sfat_form_dir_entry, file size is %u\n", le32_to_cpu(de->size));
    de->fst_cls_no = cpu_to_le32(size? start_cls: SFAT_ENTRY_FREE);

    de->crt_time =     cpu_to_le32(ts->tv_sec);
    de->lst_acc_time = cpu_to_le32(ts->tv_sec);
    de->wrt_time =     cpu_to_le32(ts->tv_sec);

}

/*
 * Desc: Find the free entry in FAT (scanning from the beginning)
 *       allocate it (initialize it by SFAT_ENTRY_EOC) once found.
 * Input:
 * Output:
 *   cls: 
 * Return:
 *   < 0: error
 *   0: success
 *
 */
int sfat_fat_entry_acquire(struct sfat_fs_info *fs, struct block_device *bdev, size_t *cls)
{
    int error = 0;

    unsigned long blk = fs->fat_start_blk;
    unsigned long blk_end = blk + fs->fat_length_blk;

    struct block_holder *bh = NULL;
    __le32 *ent = NULL;
    __le32 *ent_end = NULL;

    char * data = NULL;
    int kkk = 0;

    printk(KERN_INFO "sfat: sfat_fat_entry_acquire, fat table starts at %lu\n", blk * 512);

    bh = sfat_blkholder_alloc();
    if (!bh) {
        return -ENOMEM;
    }
    // data = sfat_blkholder_get_data(bh);

    while (blk < blk_end)
    {
        error = read_block(bdev, bh, fs->block_size, blk);

        if (error)
        {
            sfat_blkholder_free(bh);
            return -EIO;
        }
        data = sfat_blkholder_get_data(bh);
        ent = (__le32 *)data;
        ent_end = (__le32 *)(data + fs->block_size);

        if (0 == kkk)
        {
            printk(KERN_INFO "sfat: sfat_fat_entry_acquire  ent[0] is %x\n", le32_to_cpu(*(ent+0)));
            printk(KERN_INFO "sfat: sfat_fat_entry_acquire  ent[1] is %x\n", le32_to_cpu(*(ent+1)));
            printk(KERN_INFO "sfat: sfat_fat_entry_acquire  ent[2] is %x\n", le32_to_cpu(*(ent+2)));
            ++kkk;
        }

        while (ent < ent_end)
        {
            if (le32_to_cpu(*ent) == 0)
            {
                // printk(KERN_INFO "sfat: sfat_fat_entry_acquire  SFAT_ENTRY_FREE is %x\n", SFAT_ENTRY_FREE);

                // printk(KERN_INFO "sfat: sfat_fat_entry_acquire  ent is 0\n");
            }
            else
            {
                // printk(KERN_INFO "sfat: sfat_fat_entry_acquire  ent is %x\n", le32_to_cpu(*ent));
            }


            if (SFAT_ENTRY_FREE == le32_to_cpu(*ent))
            {
                printk(KERN_INFO "sfat: sfat_fat_entry_acquire  0050\n");
                *ent = cpu_to_le32(SFAT_ENTRY_EOC);
                error = write_block(bdev, bh, fs->block_size, blk);
                if (error)
                {
                    printk(KERN_INFO "sfat: sfat_fat_entry_acquire  0070\n");
                    sfat_blkholder_free(bh);
                    return -EIO;
                }
                *cls = ((blk - fs->fat_start_blk) << (fs->block_bits - 2)) + (ent - (__le32 *)data);
                printk(KERN_INFO "sfat: sfat_fat_entry_acquire  ret cls is %u\n", *cls);
                sfat_blkholder_free(bh);
                return 0;
            }
            ++ent;
        }
        ++blk;
    }

    sfat_blkholder_free(bh);
    return -ENOENT;
}

/*
 * In:
 *   cls: the no. of entry in FAT
 *   attr: the new value in that entry
 * Out:
 *   0: success
 *   < 0: error code
 * Desc: change the content of certain entry in FAT
 *
 *
 */
int sfat_fat_entry_modify(struct sfat_fs_info *fs, struct block_device *bdev,
                                size_t cls, __le32 attr)
{
    size_t blk = 0; // block
    size_t pos = 0; // entry location in the block in bytes

    struct block_holder *bh = NULL;
    int error = 0;
    __le32 *ent = NULL;

    if (cls >= fs->clusters - 1) {
        return -EINVAL;
    }

    blk = cls >> (fs->block_bits - 2); // one fat entry need 4 bytes
    pos = (cls - (blk << (fs->block_bits - 2))) << 2;

    blk = blk + fs->fat_start_blk;

    bh = sfat_blkholder_alloc();
    if (!bh) {
        return -ENOMEM;
    }

    error = read_block(bdev, bh, fs->block_size, blk);
    if (error) {
        sfat_blkholder_free(bh);
        return error;
    }

    ent = (__le32*)(sfat_blkholder_get_data(bh) + pos);
    *ent = attr;
    
    error = write_block(bdev, bh, fs->block_size, blk);
    if (error) {
        sfat_blkholder_free(bh);
        return error;
    }

    sfat_blkholder_free(bh);
    return 0;
}

/*
 * Desc: modify FAT to append a new cluster to the chain of the file
 * In:
 *   start_cls: the starting FAT entry of the file
 *   end_cls: the newly allocated entry, which will be appended to the chain of the file
 * Out:
 *   0: success
 *   < 0: error code
 */
int sfat_file_append_cls(struct sfat_fs_info *fs, struct block_device *bdev, size_t start_cls, size_t end_cls)
{
    int error = 0;
    size_t cur_cls = start_cls;
    size_t next_cls = start_cls;
    unsigned long counter = fs->clusters;

    while (counter > 0)  // just an extra protection
    {
        --counter;
        error = sfat_get_entry_content(fs, bdev, cur_cls, &next_cls);
        if (error < 0)
        {
            return error;
        }
        else
        {
            if (SFAT_ENTRY_EOC == next_cls)
            {
                return sfat_fat_entry_modify(fs, bdev, cur_cls, cpu_to_le32(end_cls));

            }
            else if (next_cls > fs->clusters)  // stop prematurely
            {
                return -EINVAL;
            }
            else
            {
                cur_cls = next_cls;
            }
        }
    }
    return -EINVAL;
}


/* doesn't deal with root inode */
/*
 * fill an inode (along with inode_info) based on the information in dir_entry
 * and the position info
 *
 * in:
 *   i_pos: position of entry in the volume in byte
 */
static int sfat_fill_inode(struct inode *inode, struct sfat_dir_entry *de, loff_t i_pos)
{
    struct sfat_sb_info *sbi = SFAT_SB(inode->i_sb);
    struct sfat_fs_info *fs = &sbi->fs_info;
    struct sfat_inode_info *inode_info = SFAT_I(inode);

    int subfiles = 0;

    inode->i_uid = sbi->options.fs_uid;
    inode->i_gid = sbi->options.fs_gid;
    inode->i_version++;
    inode->i_generation = get_seconds();

    printk(KERN_INFO "sfat: sfat_fill_inode, file size is %u\n", le32_to_cpu(de->size));
    if (de->attr & SFAT_ATTR_DIR) {  // is a directory
        inode->i_generation &= ~1;  // This line is copied from FAT, no reason why
        inode->i_mode = sfat_make_mode(sbi, de->attr, S_IRWXUGO);
        inode->i_op = &sfat_dir_inode_operations;
        inode->i_fop = &sfat_dir_file_operations;

        inode->i_size = le32_to_cpu(de->size);

        subfiles = sfat_count_subdirs(inode);
        if (subfiles < 0) {
            subfiles = 0;  // todo error checking
        }
        inode->i_nlink = subfiles;
    } else { /* not a directory */
        inode->i_generation |= 1;
        inode->i_mode = sfat_make_mode(sbi, de->attr, S_IRWXUGO);
        inode->i_op = &sfat_file_inode_operations;
        inode->i_fop = &sfat_file_file_operations;

        inode->i_size = le32_to_cpu(de->size);
    }

    // no. of blocks consumed by the file
    inode->i_blocks = ((inode->i_size + (fs->cluster_size - 1))
               & ~((loff_t)fs->cluster_size - 1)) >> fs->block_bits;

    inode->i_mtime.tv_sec = le32_to_cpu(de->wrt_time);
    inode->i_ctime.tv_sec = le32_to_cpu(de->crt_time);
    inode->i_atime.tv_sec = le32_to_cpu(de->lst_acc_time);

    inode_info->i_start = le32_to_cpu(de->fst_cls_no);
    inode_info->i_attrs = de->attr;
    inode_info->i_pos = i_pos;
    return 0;
}

/*
 * Desc: Build an inode
 * In:
 *   de: dir entry used to fill the info for the inode
 * Return:
 *   error: 0 succ
 *          else error code
 *   output: pinode
 *
 */
int sfat_build_inode(struct super_block *sb,
            struct sfat_dir_entry *de, loff_t i_pos, struct inode **pinode)
{
    struct inode *inode;
    int error;

    printk (KERN_INFO "sfat: sfat_build_inode, i_pos is %llu\n", i_pos);
    inode = new_inode(sb);
    if (!inode) {
        return -ENOMEM;
    }
    inode->i_ino = iunique(sb, SFAT_ROOT_INO);
    inode->i_version = 1;
    error = sfat_fill_inode(inode, de, i_pos);
    if (error) {
        iput(inode);
        return -ENOMEM;
    }

    insert_inode_hash(inode);  // vfs function
    *pinode = inode;
    return 0;
}

/*
 * Desc: flush the info of inode onto hd
 * In:
 *   inode: inode info
 * Return:
 *   0: Success
 *   < 0: error code
 *
 */
int sfat_inode_write_to_hd(struct sfat_fs_info *fs, struct block_device *bdev, struct inode *inode)
{
    struct sfat_inode_info *inodei = SFAT_I(inode);
    struct sfat_dir_entry * de = NULL;
    size_t blk = 0; // block
    size_t pos = 0; // entry location in the block in bytes
    int error = 0;
    struct block_holder *bh = NULL;

    printk(KERN_INFO "sfat: sfat_inode_write_to_hd\n");

    if (SFAT_ROOT_INO == inode->i_ino)
    {
        return 0;  // no need to write root inode
    }

    printk(KERN_INFO "sfat: sfat_inode_write_to_hd, i_pos is %llu\n", inodei->i_pos);
    blk = (inodei->i_pos) >> (fs->block_bits);
    printk(KERN_INFO "sfat: sfat_inode_write_to_hd, blk is %u\n", blk);
    pos = (inodei->i_pos) & (fs->block_size - 1);
    printk(KERN_INFO "sfat: sfat_inode_write_to_hd, pos is %u\n", pos);

    bh = sfat_blkholder_alloc();
    if (!bh) {
        return -ENOMEM;
    }

    error = read_block(bdev, bh, fs->block_size, blk);
    printk(KERN_INFO "sfat: sfat_inode_write_to_hd  0030\n");
    if (error) {
        sfat_blkholder_free(bh);
        return error;
    }

    de = (struct sfat_dir_entry *)(sfat_blkholder_get_data(bh) + pos);

    // update the entry
    de->fst_cls_no = cpu_to_le32(inodei->i_start);
    de->size = cpu_to_le32(inode->i_size);
    de->crt_time =     cpu_to_le32(inode->i_ctime.tv_sec);
    de->lst_acc_time = cpu_to_le32(inode->i_atime.tv_sec);
    de->wrt_time =     cpu_to_le32(inode->i_mtime.tv_sec);

    error = write_block(bdev, bh, fs->block_size, blk);
    printk(KERN_INFO "sfat: sfat_inode_write_to_hd  0080\n");
    if (error) {
        sfat_blkholder_free(bh);
        return error;
    }

    printk(KERN_INFO "sfat: sfat_inode_write_to_hd  0100\n");
    sfat_blkholder_free(bh);
    return 0;
}




/*
 * The following operations are conveyed by super_block for
 * operating inode.
 */
// ====================================================
/*
 * the generic code from Linux would initialize the
 * content of inode after the function returns
 */
struct inode *sfat_alloc_inode(struct super_block *sb) {
    struct sfat_inode_info *ei;

    printk(KERN_INFO "sfat: sfat_alloc_inode\n");

    ei = kmem_cache_alloc(sfat_cache_inodeinfo, GFP_NOFS);
    if (!ei)
        return NULL;
    return &ei->vfs_inode;
}

/*
 * This function shall be invoked on those inodes which
 * correspond to the deleted files.
 */
void sfat_delete_inode(struct inode *inode) {
    printk(KERN_INFO "sfat: sfat_delete_inode\n");

    // quoted from fs/inode.c
    /* Filesystems implementing their own
     * s_op->delete_inode are required to call
     * truncate_inode_pages and clear_inode()
     * internally */

    // The following line doesn't do much since
    // we didn't use address space at all.
    truncate_inode_pages(&inode->i_data, 0);

    // inode->i_size = 0; todo clean the file content
    // fat_truncate(inode);
    clear_inode(inode);
}

/*
 * This function shall be invoked whenever an inode
 * is to be released.
 * This function is called before destroy_inode.
 */
void sfat_clear_inode(struct inode *inode) {
    printk(KERN_INFO "sfat: sfat_clear_inode\n");
    // todo
    // so far I am not sure what needs to be done here.

//    fat_cache_inval_inode(inode);
//    fat_detach(inode);
}

/*
 * This function shall be invoked whenever an inode
 * is to be released.
 */
void sfat_destroy_inode(struct inode *inode) {
    printk(KERN_INFO "sfat: sfat_destroy_inode\n");
    kmem_cache_free(sfat_cache_inodeinfo, SFAT_I(inode));
}


/***** Create a normal file (not directory) */
int sfat_create_file(struct inode *dir, struct dentry *dentry, int mode,
            struct nameidata *nd)
{
    struct super_block *sb = dir->i_sb;
    struct block_device *bdev = sb->s_bdev;
    struct sfat_sb_info *sbi = SFAT_SB(sb);
    struct sfat_inode_info *inodei = SFAT_I(dir);
    struct sfat_fs_info *fs_info = &sbi->fs_info;

    struct block_holder *bh = NULL;

    struct sfat_dir_entry de;
    struct sfat_dir_entry *pde = NULL;

    struct inode *inode = NULL;
    struct timespec ts;
    int error = 0;
    int i = 0;
    size_t len = (dentry->d_name.len < SFAT_NAME_LEN)? dentry->d_name.len: SFAT_NAME_LEN;

    size_t cls, blk, offset = 0;
    loff_t i_pos = 0;  // position of entry in the volume in byte
    size_t next_cls, next_blk = 0;

    int is_empty_end = 0;

    printk (KERN_INFO "sfat: sfat_create_file\n");

    for (i = 0; i < len; ++i)
    {
        de.name[i] = dentry->d_name.name[i];
    }

    for (;i < SFAT_NAME_LEN; ++i)
    {
        de.name[i] = '\0';
    }

    error = sfat_dentry_locate(fs_info, bdev, inodei->i_start, de.name,
             &de, &cls, &blk, &offset);
    if (!error)  // file exists
    {
        return -EEXIST;
    }
    if (error != -ENOENT)  // error other than file not exist
    {
        return error;
    }

    error = 0;

    bh = sfat_blkholder_alloc();
    if (!bh) {
        return -ENOMEM;
    }

    // find free entry
    error = sfat_free_dentry_locate(fs_info, bdev, bh, inodei->i_start, &cls, &blk, &offset);

    if (error && error != -ENOENT)  // if error is not "not found"
    {
        sfat_blkholder_free(bh);
        return error;
    }

    ts = CURRENT_TIME_SEC;

    if (!error)  // We found a free entry. bh contains the whole block.
    {
        i_pos = form_dir_entry_pos(fs_info, cls, blk, offset);
        printk (KERN_INFO "sfat: sfat_create_file, i_pos is %llu\n", i_pos);

        pde = (struct sfat_dir_entry *)(sfat_blkholder_get_data(bh) + offset);
        if (SFAT_ATTR_EMPTY_END == pde->attr)  // last valid entry
        {
            // more entries in the block
            if (offset < fs_info->block_size - sizeof(struct sfat_dir_entry))
            {
                (pde + 1)->attr = SFAT_ATTR_EMPTY_END;
            }
            else
            {
                is_empty_end = 1;
            }
        }

        // We update the block.
        sfat_form_dir_entry(pde, 0/* common file */, de.name,
                0/*choose at will due to size = 0*/, 0, &ts);
        memcpy(&de, pde, sizeof(struct sfat_dir_entry));

        error = write_block(bdev, bh, fs_info->block_size, CLS_TO_BLK(fs_info, cls) + blk);
        if (error)
        {
            sfat_blkholder_free(bh);
            return error;
        }

        // only need to change the time for directory
        dir->i_mtime.tv_sec = ts.tv_sec;
        // inode->i_atime.tv_sec = ts;  // I didn't change the access time

        // modify the first entry of next block (if it exists)
        if (is_empty_end)
        {
            // more blocks in the cluster
            if (blk < fs_info->blk_per_clus - 1)
            {
                next_blk = blk + 1;
            }
            else  // more cluster in the chain
            {
                error = sfat_get_entry_content(fs_info, bdev, cls, &next_cls);
                if (error)
                {
                    sfat_blkholder_free(bh);
                    return error;
                }
                // last the cluster
                else if (SFAT_ENTRY_EOC == next_cls)
                {
                    is_empty_end = 0;
                }
                // mysterious error
                else if (next_cls > fs_info->clusters)
                {
                    sfat_blkholder_free(bh);
                    return error;
                }
                else
                {
                    next_blk = 0;
                }
            }

            if (is_empty_end)
            {
                error = read_block(bdev, bh, fs_info->block_size, CLS_TO_BLK(fs_info, next_cls) + next_blk);
                if (error)
                {
                    sfat_blkholder_free(bh);
                    return error;
                }
                pde = (struct sfat_dir_entry *)(sfat_blkholder_get_data(bh));
                pde->attr = SFAT_ATTR_EMPTY_END;
                error = write_block(bdev, bh, fs_info->block_size, CLS_TO_BLK(fs_info, next_cls) + next_blk);
                if (error)
                {
                    sfat_blkholder_free(bh);
                    return error;
                }
            }
        }
    }
    else  // error is -ENOENT (no free entry)
    {
        // allocate one entry
        // I am in a hurry. If something bad happens later, I
        // don't return it back.
        error = sfat_fat_entry_acquire(fs_info, bdev, &cls);
        if (error)
        {
            printk (KERN_INFO "sfat: sfat_create_file, no free entry in FAT.\n");
            sfat_blkholder_free(bh);
            return error;
        }

        // add the newly allocated cluster to the chain of the dir
        error = sfat_file_append_cls(fs_info, bdev, inodei->i_start, cls);
        if (error)
        {
            sfat_blkholder_free(bh);
            return error;
        }
        // update size and time of the directory
        dir->i_size += fs_info->cluster_size;
        dir->i_blocks += fs_info->blk_per_clus;
        // change the time for directory
        dir->i_mtime.tv_sec = ts.tv_sec;
        // inode->i_atime.tv_sec = ts.tv_sec;  // I didn't change the access time

        error = read_block(bdev, bh, fs_info->block_size, CLS_TO_BLK(fs_info, cls));
        if (error)
        {
            sfat_blkholder_free(bh);
            return error;
        }

        i_pos = form_dir_entry_pos(fs_info, cls, blk, offset);

        // We update the block.
        pde = (struct sfat_dir_entry *)(sfat_blkholder_get_data(bh));
        // write down the entry (as the first one in the cluster) for the new file
        sfat_form_dir_entry(pde, 0/* common file */, de.name,
                0/*choose at will due to size = 0*/, 0, &ts);
        memcpy(&de, pde, sizeof(struct sfat_dir_entry));

        ++pde;
        // change the next entry
        pde->attr = SFAT_ATTR_EMPTY_END;
        error = write_block(bdev, bh, fs_info->block_size, CLS_TO_BLK(fs_info, cls));
        if (error)
        {
            sfat_blkholder_free(bh);
            return error;
        }
    }

    // so far the meta data of the dir (inside the inode)
    // as well as the fat chain of the dir have been updated.

    error = sfat_inode_write_to_hd(fs_info, bdev, dir);
    sfat_blkholder_free(bh);
    if (error)
    {
        return error;
    }

    error = sfat_build_inode(sb, &de, i_pos, &inode);

    if (error) {
        return error;
    }

    d_instantiate(dentry, inode);
    return 0;
}


/*
 * Desc:
 *   look up certain file in a directory according to file's name
 *
 * In:
 *   dir: directory
 *   dentry: dentry prepared for file to be looked up.
 *   data: name info of the file to be looked up.
 *
 * Ret:
 *   NULL: no such file
 *   not NULL (< 0): error code formed by ERR_PTR(err)
 *
 */
struct dentry * sfat_lookup(struct inode *dir,struct dentry *dentry, struct nameidata *data)
{
    struct super_block *sb = dir->i_sb;
    struct block_device *bdev = sb->s_bdev;
    struct sfat_sb_info *sbi = SFAT_SB(sb);
    struct sfat_inode_info *inodei = SFAT_I(dir);
    struct sfat_fs_info *fs_info = &sbi->fs_info;

    struct inode *inode = NULL;

    struct sfat_dir_entry de;
    size_t len = (dentry->d_name.len < SFAT_NAME_LEN)? dentry->d_name.len: SFAT_NAME_LEN;

    size_t cls, blk, offset = 0;
    loff_t i_pos = 0;  // position of entry in the volume in byte

    int i = 0;
    int error = 0;

    printk (KERN_INFO "sfat: sfat_lookup\n");

    for (i = 0; i < len; ++i)
    {
        de.name[i] = dentry->d_name.name[i];
    }

    for (;i < SFAT_NAME_LEN; ++i)
    {
        de.name[i] = '\0';
    }

    error = sfat_dentry_locate(fs_info, bdev, inodei->i_start, de.name,
             &de, &cls, &blk, &offset);
    if (error)
    {
        if (-ENOENT == error)
        {
            return NULL;
        }
        else
        {
            return ERR_PTR(error);
        }
    }

    i_pos = form_dir_entry_pos(fs_info, cls, blk, offset);

    error = sfat_build_inode(sb, &de, i_pos, &inode);
    if (error) {
        return ERR_PTR(error);
    }

    // The following code is copied from msdos(namei_msdos.c)
    dentry->d_op = &sfat_dentry_operations;
    dentry = d_splice_alias(inode, dentry);
    if (dentry)
        dentry->d_op = &sfat_dentry_operations;
    return dentry;
}


// todo: to be implemented
int sfat_setattr(struct dentry *de, struct iattr *attr)
{
    printk (KERN_INFO "sfat: sfat_setattr\n");
    return -EINVAL;
}

int sfat_getattr(struct vfsmount *mnt, struct dentry *dentry, struct kstat *stat)
{
    struct inode *inode = dentry->d_inode;

    printk (KERN_INFO "sfat: sfat_getattr, inode no. is %lu\n", inode->i_ino);

    generic_fillattr(inode, stat);  // linux library function
    // stat->blksize = SFAT_SB(inode->i_sb)->fs_info.cluster_size;  // copied from FAT
    stat->blksize = SFAT_SB(inode->i_sb)->fs_info.block_size;

    printk (KERN_INFO "sfat: sfat_getattr \n"
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


/*
 * Desc: fill the dirent as much as possible, may add more than one dir
 * Para:
 *   In:
 *   dirent: struct kernel uses to contain direntry
 *   filldir: function kernel provides for others to use to fill the dir entry
 *
 *   Return:
 *       0: O.K.
 *       < 0: error code
 */
int sfat_readdir(struct file *filp, void *dirent, filldir_t filldir) {
    struct inode *inode = filp->f_path.dentry->d_inode;
    struct super_block *sb = inode->i_sb;
    struct block_device *bdev = sb->s_bdev;
    struct sfat_sb_info *sbi = SFAT_SB(sb);
    struct sfat_inode_info *inodei = SFAT_I(inode);
    struct sfat_fs_info *fs_info = &sbi->fs_info;

    int error = 0;
    size_t cls = inodei->i_start;
    size_t size = inode->i_size; // type changing from loff_t to size_t
                                 // since the size in dir entry has only 32-bit

    size_t rounds = 1 + ((size + fs_info->cluster_size - 1) >> fs_info->cluster_bits);

    size_t blk = 0;

    struct block_holder *bh = NULL;

    char *data = NULL;
    struct sfat_dir_entry *de = NULL;

    ino_t inode_no = 0;
    size_t len = 0;
    loff_t cpos = filp->f_pos;  // pos in the directory file
    size_t offset = 0;
    size_t blk_off = 0;
    // --------------------------
    printk(KERN_INFO "sfat: sfat_readdir\n");

    if (size < 32) {
        return 0; // empty directory
    }
    if (cpos >= size) {  // nothing left to read
        return -EINVAL;
    }

    // Boundary checking
    if (cpos & (sizeof(struct sfat_dir_entry) - 1)) {
        return -ENOENT;
    }

    error = sfat_seek(fs_info, bdev, cls, cpos, &cls, &offset);
    if (error)
    {
        return error;
    }

    bh = sfat_blkholder_alloc();
    if (!bh) {
        return -ENOMEM;
    }

    blk = CLS_TO_BLK(fs_info, cls);
    blk_off = (offset >> fs_info->block_bits);  // in one cluster
    offset = offset & (fs_info->block_size - 1);  // in one block

    // rounds -> cluster
    // blk_off -> block
    // offset -> entry
    while (rounds > 0) {  // just for protection of loop
        --rounds;

        while (blk_off < fs_info->blk_per_clus)
        {
            error = read_block(bdev, bh, fs_info->block_size, blk + blk_off);
            if (error) {
                goto outloop;
            }
            data = sfat_blkholder_get_data(bh);
            while (offset < fs_info->block_size)
            {
                de = (struct sfat_dir_entry *) (data + offset);
                if (de->attr & SFAT_ATTR_EMPTY_END) {
                    cpos += sizeof(struct sfat_dir_entry);  // adjust cpos (increase cpos gradually)
                    goto outloop;
                } else if (de->attr & SFAT_ATTR_EMPTY) {
                    cpos += sizeof(struct sfat_dir_entry);  // adjust cpos
                    offset += sizeof(struct sfat_dir_entry);
                    continue;
                } else {  // certain valid entry
                    len = str_len(de->name, 11);
                    inode_no = iunique(sb, SFAT_ROOT_INO);  // get a unique inode no.
                    error = filldir(dirent, de->name, len, cpos, inode_no,
                        (de->attr & SFAT_ATTR_DIR) ? DT_DIR : DT_REG);
                    if (error < 0)
                    {
                        goto outloop;
                    }
                    cpos += sizeof(struct sfat_dir_entry);
                    offset += sizeof(struct sfat_dir_entry);
                }
            }

            offset = 0;
            ++blk_off;
        }
        blk_off = 0;

        // try to find the next cluster
        error = sfat_get_entry_content(fs_info, bdev, cls, &cls);
        // read error or abnormal cluster normal
        if (error || cls > fs_info->clusters) {
            break;
        }

        blk = CLS_TO_BLK(fs_info, cls);
    }

outloop:
    // loop in the fat chain
    if (rounds == 0) {
        error = -EINVAL;
    }
    filp->f_pos = cpos;

    sfat_blkholder_free(bh);
    return error;
}

/*
 * Desc:
 *   Copy data into certain cluster from user space according to the offset in the cluster.
 *
 * In:
 *   len: length of the data to be copied. (assume offset + len <= cluster_size)
 *   offset: offset in the cluster (assume it is < cluster_size)
 * Out:
 *   error: error code for why ret < len
 *   no error code is given if copy_from_user failed
 *
 * Return: length of the data actually written (may be less than len)
 */

size_t sfat_write_cluster(struct sfat_fs_info *fs, struct block_device *bdev,
        const char __user *buf, size_t len,
        size_t cls, size_t offset, int *perror)
{
    size_t blk = CLS_TO_BLK(fs, cls) + (offset >> fs->block_bits);
    size_t blk_offset = offset & (fs->block_size - 1);

    size_t blk_space = fs->block_size - blk_offset;

    struct block_holder *bh = NULL;
    char *data = NULL;

    size_t write_len = 0;

    unsigned long copied = 0;

    size_t ret_len = 0;

    printk(KERN_INFO "sfat: sfat_write_cluster  len is %u, cls is %u, offset is %u\n", len, cls, offset);

    *perror = 0;


    bh = sfat_blkholder_alloc();
    if (!bh) {
        *perror = -ENOMEM;
        return ret_len;
    }
    data = sfat_blkholder_get_data(bh);

    if (blk_offset)  // offset isn't on the block edge
    {
        *perror = read_block(bdev, bh, fs->block_size, blk);
        if (*perror)
        {
            sfat_blkholder_free(bh);
            return ret_len;
        }

        write_len = len < blk_space? len: blk_space;
        copied = copy_from_user(data + blk_offset, buf, write_len);
        if (copied)
        {
            sfat_blkholder_free(bh);
            return ret_len;  // don't give reason for failure of copy
        }
        *perror = write_block(bdev, bh, fs->block_size, blk);
        if (*perror)
        {
            sfat_blkholder_free(bh);
            return ret_len;
        }
        buf += write_len;
        blk++;
        ret_len += write_len;
        len -= write_len;
    }

    while (len >= fs->block_size)
    {
        copied = copy_from_user(data, buf, fs->block_size);
        if (copied)
        {
            sfat_blkholder_free(bh);
            return ret_len;  // don't give reason for failure of copy
        }
        *perror = write_block(bdev, bh, fs->block_size, blk);
        if (*perror)
        {
            sfat_blkholder_free(bh);
            return ret_len;
        }
        buf += fs->block_size;
        blk++;
        ret_len += fs->block_size;
        len -= fs->block_size;
    }

    if (len > 0)
    {
        *perror = read_block(bdev, bh, fs->block_size, blk);
        if (*perror)
        {
            sfat_blkholder_free(bh);
            return ret_len;
        }

        printk(KERN_INFO "sfat: sfat_write_cluster 0080\n");
        copied = copy_from_user(data, buf, len);
        if (copied)
        {
            printk(KERN_INFO "sfat: sfat_write_cluster 0085\n");
            sfat_blkholder_free(bh);
            return ret_len;  // don't give reason for failure of copy
        }
        printk(KERN_INFO "sfat: sfat_write_cluster 0090, data[0] is %c, data[1] is %c\n", data[0], data[1]);
        *perror = write_block(bdev, bh, fs->block_size, blk);
        printk(KERN_INFO "sfat: sfat_write_cluster 0100\n");
        if (*perror)
        {
            printk(KERN_INFO "sfat: sfat_write_cluster 0110\n");
            sfat_blkholder_free(bh);
            return ret_len;
        }
        ret_len += len;
    }

    printk(KERN_INFO "sfat: sfat_write_cluster 0220\n");
    sfat_blkholder_free(bh);
    return ret_len;
}


/*
 * Desc:
 *   Write content from user space (buf) to the volume
 *
 * In:
 *   buf: buffer in user space
 *   len: length of content to be written
 *
 * InOut:
 *   ppos: offset in the current file
 *
 * Return:
 *   >=0: no. of bytes written
 *
 */
ssize_t sfat_sync_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{
    struct inode *inode = filp->f_path.dentry->d_inode;
    struct super_block *sb = inode->i_sb;
    struct block_device *bdev = sb->s_bdev;
    struct sfat_sb_info *sbi = SFAT_SB(sb);
    struct sfat_inode_info *inodei = SFAT_I(inode);
    struct sfat_fs_info *fs_info = &sbi->fs_info;

    size_t fsize = inode->i_size; // type changing from loff_t to size_t
                                 // since the size in dir entry has only 32-bit

    size_t cur_cls = inodei->i_start;
    size_t new_cls = 0;
    size_t offset = 0;

    size_t write_space = 0;
    size_t write_len = 0;
    size_t ret_len = 0;
    size_t accu_len = 0;

    struct timespec ts;

    int error = 0;
    // --------------------------
    printk(KERN_INFO "sfat: sfat_sync_write, file size is %u\n", fsize);
    printk(KERN_INFO "sfat: sfat_sync_write  *ppos is %lld, len is %u\n", *ppos, len);


    if (*ppos > fsize || len == 0)  // don't allow null write
    {
        return -EINVAL;  // todo Currently no gap is allowed.
    }

    // file size is not multiple of block size
    // or
    // offset is not at the end
    if ((fsize & (fs_info->block_size - 1)) || (*ppos < fsize))
    {
        error = sfat_seek(fs_info, bdev, cur_cls, *ppos, &cur_cls, &offset);
        if (error)
        {
            return error;
        }
        write_space = fs_info->cluster_size - offset;
        write_len = len > write_space? write_space: len;
        ret_len = sfat_write_cluster(fs_info, bdev, buf, write_len,
                cur_cls, offset, &error);
        *ppos += ret_len;
        len -= ret_len;
        buf += ret_len;
        accu_len += ret_len;
        if (ret_len < write_len)
        {
            goto end;
        }

        while (len > 0)
        {
            error = sfat_get_entry_content(fs_info, bdev, cur_cls, &cur_cls);
            if (error)
            {
                goto end;
            }
            if (cur_cls < SFAT_ENTRY_MAX)
            {
                write_space = fs_info->cluster_size;
                write_len = len > write_space? write_space: len;
                ret_len = sfat_write_cluster(fs_info, bdev, buf, write_len,
                        cur_cls, 0, &error);
                *ppos += ret_len;
                len -= ret_len;
                buf += ret_len;
                accu_len += ret_len;
                if (ret_len < write_len)
                {
                    goto end;
                }
            }
            else
            {
                break;  // no more cluster in the chain
            }
        }
    }
    else // file size is multiple of block size and offset is at the end
    {
        if (fsize > 0)
        {
            // seek to the last cluster
            error = sfat_seek(fs_info, bdev, cur_cls, *ppos - 1, &cur_cls, &offset);
            if (error)
            {
                goto end;
            }
        }
        else  // fsize = 0;
        {
            printk(KERN_INFO "sfat: sfat_sync_write  0030\n");
            error = sfat_fat_entry_acquire(fs_info, bdev, &new_cls); // allocate one cluster to the file
            if (error)
            {
                printk(KERN_INFO "sfat: sfat_sync_write  0040\n");
                goto end;
            }
            else
            {
                cur_cls = new_cls;
                write_space = fs_info->cluster_size;
                write_len = len > write_space? write_space: len;
                ret_len = sfat_write_cluster(fs_info, bdev, buf, write_len,
                        cur_cls, 0, &error);
                *ppos += ret_len;
                len -= ret_len;
                buf += ret_len;
                accu_len += ret_len;
                if (ret_len < write_len)
                {
                    if (ret_len > 0)
                    {
                        printk(KERN_INFO "sfat: sfat_sync_write  0061\n");
                        inodei->i_start = cur_cls;
                    }
                    else
                    {
                        printk(KERN_INFO "sfat: sfat_sync_write  0063\n");
                        // todo I didn't return the newly created cluster back
                    }
                    goto end;
                }
                else
                {
                    printk(KERN_INFO "sfat: sfat_sync_write  0066\n");
                    inodei->i_start = cur_cls;
                }

            }

        }
    }

    while (len > 0)  // add new cluster
    {
        error = sfat_fat_entry_acquire(fs_info, bdev, &new_cls);
        if (error)
        {
            goto end;
        }

        error = sfat_fat_entry_modify(fs_info, bdev, cur_cls, cpu_to_le32(new_cls));
        if (error)
        {
            goto end;
        }
        cur_cls = new_cls;

        write_space = fs_info->cluster_size;
        write_len = len > write_space? write_space: len;
        ret_len = sfat_write_cluster(fs_info, bdev, buf, write_len,
                cur_cls, 0, &error);
        *ppos += ret_len;
        len -= ret_len;
        buf += ret_len;
        accu_len += ret_len;
        if (ret_len < write_len)
        {
            goto end;
        }
    }

end:
    fsize = fsize < *ppos? *ppos: fsize;
    printk(KERN_INFO "sfat: sfat_sync_write  file size is %u\n", fsize);
    inode->i_size = fsize;

    ts = CURRENT_TIME_SEC;
    inode->i_mtime.tv_sec = ts.tv_sec;  // time for modification
    inode->i_atime.tv_sec = ts.tv_sec;  // time for access

    // no. of blocks consumed by the file
    inode->i_blocks = ((inode->i_size + (fs_info->cluster_size - 1))
               & ~((loff_t)fs_info->cluster_size - 1)) >> fs_info->block_bits;

    error = sfat_inode_write_to_hd(fs_info, bdev, inode);
    // don't care about the error
    return accu_len;

}


/*
 * Desc:
 *   Copy data from certain cluster into user space according to the offset in the cluster.
 *
 * In:
 *   len: length of the data to be copied. (assume offset + len <= cluster_size)
 *   offset: offset in the cluster (assume it is < cluster_size)
 * Out:
 *   error: error code for why ret < len
 *   no error code is given if copy_to_user failed
 *
 * Return: length of the data actually copied (may be less than len)
 */

size_t sfat_read_cluster(struct sfat_fs_info *fs, struct block_device *bdev,
        char __user *buf, size_t len,
        size_t cls, size_t offset, int *perror)
{
    size_t blk = CLS_TO_BLK(fs, cls) + (offset >> fs->block_bits);
    size_t blk_offset = offset & (fs->block_size - 1);
    size_t blk_space = fs->block_size - blk_offset;

    struct block_holder *bh = NULL;
    char *data = NULL;

    size_t read_len = 0;

    unsigned long copied = 0;

    size_t ret_len = 0;

    printk(KERN_INFO "sfat: sfat_read_cluster, len is %u, cls is %u, offset %u\n", len, cls, offset);
    *perror = 0;

    /* ***************************** */
    printk(KERN_INFO "sfat: sfat_read_cluster  test 0010\n");
    /* ***************************** */


    bh = sfat_blkholder_alloc();
    if (!bh) {
        *perror = -ENOMEM;
        return ret_len;
    }
    // data = sfat_blkholder_get_data(bh);

    while (len > 0)  // offset isn't on the block edge
    {
        printk(KERN_INFO "sfat: sfat_read_cluster 0030, len is %u\n", len);
        *perror = read_block(bdev, bh, fs->block_size, blk);
        data = sfat_blkholder_get_data(bh);
        if (*perror)
        {
            sfat_blkholder_free(bh);
            return ret_len;
        }

        printk(KERN_INFO "sfat: sfat_read_cluster 0040\n");
        read_len = len < blk_space? len: blk_space;
        copied = copy_to_user(buf, data + blk_offset, read_len);

        printk(KERN_INFO "sfat: sfat_read_cluster 0040, data[0] is %c, data[1] is %c\n", data[blk_offset], data[blk_offset + 1]);

        if (copied)  // Something is left uncopied.
        {
            sfat_blkholder_free(bh);
            ret_len += (read_len - copied);
            return ret_len;  // don't give reason for failure of copy
        }

        printk(KERN_INFO "sfat: sfat_read_cluster 0070\n");
        buf += read_len;
        blk++;
        ret_len += read_len;
        len -= read_len;
        blk_offset = 0;
        blk_space = fs->block_size;
    }

    printk(KERN_INFO "sfat: sfat_read_cluster 0100\n");
    sfat_blkholder_free(bh);
    return ret_len;
}


/*
 * Desc:
 *   Read content to user space (buf) from the volume
 *
 * In:
 *   buf: buffer in user space
 *   len: length of content to be read
 *
 * InOut:
 *   ppos: offset in the current file
 *
 * Return:
 *   >=0: no. of bytes written
 *
 */
ssize_t sfat_sync_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{

    struct inode *inode = filp->f_path.dentry->d_inode;
    struct super_block *sb = inode->i_sb;
    struct block_device *bdev = sb->s_bdev;
    struct sfat_sb_info *sbi = SFAT_SB(sb);
    struct sfat_inode_info *inodei = SFAT_I(inode);
    struct sfat_fs_info *fs_info = &sbi->fs_info;

    size_t fsize = inode->i_size; // type changing from loff_t to size_t
                                 // since the size in dir entry has only 32-bit

    size_t cur_cls = inodei->i_start;
    size_t new_cls = 0;
    size_t offset = 0;

    size_t read_space = 0;
    size_t read_len = 0;
    size_t accu_len = 0;

    struct timespec ts;

    int error = 0;
    // --------------------------
    printk(KERN_INFO "sfat: sfat_sync_read, file size is %u, len is %u, *ppos is %llu\n", fsize, len, *ppos);

    if (*ppos >= fsize)
    {
        return 0;
    }
    if (*ppos + len > fsize)
    {
        len = fsize - *ppos;
    }

    if (*ppos >= fs_info->cluster_size)  // todo just read the first cluster
    {
        return 0;
    }

    if (*ppos + len > fs_info->cluster_size)  // todo just read the first cluster
    {
        len = fs_info->cluster_size - *ppos;
    }

    if (0 == len)
    {
        return 0;
    }

    if (cur_cls > SFAT_ENTRY_MAX)
    {
        printk(KERN_INFO "sfat: sfat_sync_read, 00050\n");
        return 0;
    }

    accu_len = sfat_read_cluster(fs_info, bdev, buf, len, cur_cls, *ppos, &error);
    *ppos += accu_len;


    ts = CURRENT_TIME_SEC;
    inode->i_atime.tv_sec = ts.tv_sec;  // time for access

    error = sfat_inode_write_to_hd(fs_info, bdev, inode);
    // don't care about the error

    return accu_len;  // todo

}





