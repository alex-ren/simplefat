#ifndef __SFAT_FS_H
#define __SFAT_FS_H

// learn from include/linux/msdos_fs.h
/*
 * Author: Zhiqiang Ren
 * Date:   Jan. 29th. 2012
 * The header can be used by user level application.
 */

#include <linux/types.h>
//#include <linux/magic.h>
//#include <asm/byteorder.h>

/*
 * The Simple SFAT filesystem constants/structures
 */
#define SFAT_MEDIA 0x25

/* padding for the start cluster for normal file which has no cluster allocated */
/* in fat table, this means the entry is free */
#define SFAT_ENTRY_FREE 0xFFFFFFF9
/* standard EOF */
#define SFAT_ENTRY_EOC 0xFFFFFFF8
/* bad cluster mark */
#define SFAT_ENTRY_BAD 0xFFFFFFF7
/* maximum cluster */
/* just an indication of number, no real usage in fat */
#define SFAT_ENTRY_MAX 0xFFFFFFF6
/* no use now */
#define SFAT_ENTRY_EXTRA 0xFFFFFFFF


#define SFAT_ROOT_INO 5  // I like 5.



/* sfat's attribute */
#define SFAT_ATTR_NONE   0   /* no attribute bits */
//#define ATTR_RO     1   /* read-only */
//#define ATTR_HIDDEN 2   /* hidden */
//#define ATTR_SYS    4   /* system */
//#define ATTR_VOLUME 8   /* volume label */
#define SFAT_ATTR_DIR    16  /* directory */
#define SFAT_ATTR_EMPTY    64  /* this entry is free */
#define SFAT_ATTR_EMPTY_END    128  /* this entry and the rest entries are free */
//#define ATTR_ARCH   32  /* archived */

///* attribute bits that are copied "as is" */
//#define ATTR_UNUSED (ATTR_VOLUME | ATTR_ARCH | ATTR_SYS | ATTR_HIDDEN)
///* bits that are used by the Windows 95/Windows NT extended FAT */
//#define ATTR_EXT    (ATTR_RO | ATTR_HIDDEN | ATTR_SYS | ATTR_VOLUME)


#define SFAT_NAME_LEN  11  /* maximum name length */



/*
 * ioctl commands
 */
#define SFAT_IOCTL_TEST	_IO('r', 0x20)

/*
 * | head (1 sector) | reserved area (multiple sectors) | fat area (multiple sectors X 2) | data area |
 * head: 1 sector
 * reserved area: reverved
 * fat area: fat_length X fatno
 * data area: rest of the volume
 *
 */
struct sfat_boot_sector {
    // the following 2 items are copied from fat
    __u8    ignored[3];	/* Boot strap short or near jump */
    __u8    system_id[8];	/* Name - can be used to special case
                            partition manager volumes */

    __u8    media;  /* media code */

    __le16  sector_size;	/* bytes per logical sector */
    __u8    sec_per_clus;	/* sectors per cluster */
    __le16  reserved;	/* no. of reserved sectors */
    __le32  fat_length;	/* no. of sectors in one FAT */
    __u8    fats;		/* number of FATs */

    __le32  sectors;        /* number of sectors in the volume */
    __le32  clusters;       /* number of clusters in the data area */
                            /* also the no. of valid entries in FAT */
    __le32  root_start;           /* cluster no. of the root directory */
    __le32  root_size;           /* size of the root directory in cluster */
    __le32  freelist;       /* cluster no. of the free list */  // todo This one is useless.
}__attribute__((__packed__));

struct sfat_dir_entry {  // 32 bytes
	__u8    name[11];  // name of file (may or may not contain '\0')
                           // name[0] == 0 => free entry, name[0] == name[1] == 0 => all of the rest are free

	__u8    attr;  // Attribute

	__le32  crt_time;  // timespec ts;   crt_time = cpu_to_le32(ts->tv_sec);
	__le32  lst_acc_time;  // last access time
	__le32  wrt_time;  // last write time

	__le32  size;  // file size
	__le32  fst_cls_no;  // first cluster no.  If size == 0, then this is SFAT_ENTRY_FREE(0)
}__attribute__((__packed__));



#endif

