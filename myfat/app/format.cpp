
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>  // for ioctl

#include <string.h>

#include <stdlib.h>  // for exit

#include <linux/fs.h>
#include <linux/types.h>

#include <cstdio>
#include <iostream>
#include <string>
#include <exception>
#include <cerrno>

#include "../simplefat/sfat_fs.h"
#include "BlockDevice.h"

using std::cerr;
using std::cout;
using std::cin;
using std::endl;
using std::string;
using std::exception;

#define ELE_BLOCK_SZ 512
#define BYTES_PER_SECTOR  512
#define SECTORS_PER_CLUSTER 4
#define RESERVE_SECTORS 10
#define SFAT_NO 2  // two fat tables on the disk


/*
 * All counting for sector or cluster starts from 0.
 */


void ioctl_get_msg(int file_desc);

int main (int argc, char *argv[])
{
    int x = 3 * 6;
    string name;
    if (argc < 2)
    {
        name = "/dev/loop1";
    }
    else
    {
        name = argv[1];
    }

    cout << "file name: " << name << endl;
    // cin >> str;

    try
    {
        BlockDevice bdev(name);
        cout << "getsize64 is " << bdev.getsize64() << endl;
        cout << "getss is " << bdev.getss() << endl;
        cout << "getpbsz is " << bdev.getpbsz() << endl;
        cout << "getbsz is " << bdev.getbsz() << endl;
        cout << "getsize is " << bdev.getsize() << endl;

        size_t blk_sector_sz = bdev.getss();  // I am not so sure whether this is good. 
                                              // But I just chose this one as the block size.
        size_t sectors = bdev.getsize() / (BYTES_PER_SECTOR / blk_sector_sz);  // at most 2^32 - 1 sectors
                                                                            // no. of sectors for fat

        size_t clusters = (sectors - 1 /*super_sector*/ - RESERVE_SECTORS) / SECTORS_PER_CLUSTER;
        if (clusters > SFAT_ENTRY_MAX)
        {
        	clusters = SFAT_ENTRY_MAX;  // only support part of the hard disk
        }

        size_t fat_length_sectors = (clusters * 4/*bytes*/ + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR;

        size_t fat_length_clusters = 
            (SFAT_NO * fat_length_sectors + SECTORS_PER_CLUSTER - 1) / SECTORS_PER_CLUSTER;
        clusters = clusters - fat_length_clusters;

        struct sfat_boot_sector super_sector = {
        //    .media = 0x25,
        //    .sec_per_clus = SECTORS_PER_CLUSTER,
        //    .fats = SFAT_NO
        };
        super_sector.media = SFAT_MEDIA;
        super_sector.sector_size = static_cast<__le16>(BYTES_PER_SECTOR);  // todo: use better 
        super_sector.sec_per_clus = SECTORS_PER_CLUSTER;
        super_sector.reserved = static_cast<__le16>(RESERVE_SECTORS);
        super_sector.fat_length = static_cast<__le32>(fat_length_sectors);
        super_sector.fats = SFAT_NO;

        super_sector.sectors = static_cast<__le32>(sectors);
        super_sector.clusters = static_cast<__le32>(clusters);

        // starting cluster of root is 0
        super_sector.root_start = static_cast<__le32>(0);
        // size of root is 1 cluster
        super_sector.root_size = static_cast<__le32>(1);
        
        cout << "size of struct is " << sizeof(sfat_boot_sector) << endl;
        ssize_t ret = bdev.write(&super_sector, sizeof(sfat_boot_sector));
        if (ret < sizeof(sfat_boot_sector))
        {
            cerr << "write failed, errno is " << errno << " , info is: " << 
                strerror(errno) << endl;
            throw std::runtime_error("write error");
        }
        
        #define ROUND_SIZE 512  // we write 512 bytes each time
        #define ENTRIES (ROUND_SIZE / sizeof(__le32))

        cout << "ENTRIES is " << ENTRIES << endl;
        // preparing to write the fat table
        off_t oft = bdev.lset((1 + RESERVE_SECTORS) * BYTES_PER_SECTOR);
        if (oft == static_cast<off_t>(-1))
        {
            cerr << "lset failed, errno is " << errno << " , info is: " << 
                strerror(errno) << endl;
            throw std::runtime_error("lset error");
        }
        
        // write the fat tables
        size_t rounds = fat_length_sectors * (BYTES_PER_SECTOR / ROUND_SIZE);
        __le32 fat_block[ENTRIES] = {};
        cout << "size of " << sizeof(fat_block) << endl;

        for (size_t i = 0; i < ENTRIES; ++i)
        {
            fat_block[i] = SFAT_ENTRY_FREE;
        }
        cout << "SFAT_ENTRY_FREE is " << std::hex << SFAT_ENTRY_FREE << endl;

        for (int i = 0; i < SFAT_NO; ++i)
        {
            // root dir consumes only one cluster
            fat_block[0] = static_cast<__le32>(SFAT_ENTRY_EOC);  

            ssize_t ret = bdev.write(fat_block, ROUND_SIZE);
            if (ret < ROUND_SIZE)
            {
                cerr << "write failed, errno is " << errno << " , info is: " << 
                    strerror(errno) << endl;
                throw std::runtime_error("write error");
            }

            fat_block[0] = static_cast<__le32>(SFAT_ENTRY_FREE);  
            for (size_t cur_round = 1; cur_round < rounds; ++cur_round)
            {
                ssize_t ret = bdev.write(fat_block, ROUND_SIZE);
                if (ret < ROUND_SIZE)
                {
                    cerr << "write failed, errno is " << errno << " , info is: " << 
                        strerror(errno) << endl;
                    throw std::runtime_error("write error");
                }
            }
        }

        // write the dir entry of root, which is adject to FAT table
        struct sfat_dir_entry *pDirEntry = reinterpret_cast<sfat_dir_entry *>(fat_block);
        pDirEntry->attr = SFAT_ATTR_EMPTY_END;

        ret = bdev.write(fat_block, ROUND_SIZE);
        if (ret < ROUND_SIZE)
        {
            cerr << "write failed, errno is " << errno << " , info is: " << 
                strerror(errno) << endl;
            throw std::runtime_error("write error");
        }

        bdev.lset(0);
        struct sfat_boot_sector temp;
        ret = bdev.read(&temp, sizeof(sfat_boot_sector));
        if (ret < sizeof(sfat_boot_sector))
        {
            cerr << "read failed, errno is " << errno << " , info is: " << 
                strerror(errno) << endl;
            throw std::runtime_error("write error");
        }
        cout << "temp.media is " << static_cast<int>(temp.media) << endl;
        cout << "super_sector.media is " << static_cast<int>(super_sector.media) << endl;
        
        
        bdev.lset(0);


        bdev.lset((1 + RESERVE_SECTORS) * BYTES_PER_SECTOR);
        cout << "fat table starts at " << std::dec << (1 + RESERVE_SECTORS) * BYTES_PER_SECTOR << endl;
        cout << "sizeof(fat_block) is " << std::dec << sizeof(fat_block) << endl;
        ret = bdev.read(fat_block, sizeof(fat_block));
        if (ret < sizeof(fat_block))
        {
            cerr << "read failed, errno is " << errno << " , info is: " <<
                strerror(errno) << endl;
            throw std::runtime_error("write error");
        }

        cout << "fat_block[0] is " << std::hex << static_cast<unsigned int>(fat_block[0]) << endl;
        cout << "fat_block[1] is " << std::hex << static_cast<unsigned int>(fat_block[1]) << endl;
        cout << "fat_block[2] is " << std::hex << static_cast<unsigned int>(fat_block[2]) << endl;


        bdev.lset(0);


    }
    catch(const std::exception &e)
    {
        cerr << "exception caught: " << e.what() << endl;
    }

    return 0;
}

void ioctl_get_msg(int file_desc)
{
    long ret_val = 0;
    unsigned long long file_size_in_bytes = 0;
    int blksz = 0;
    unsigned long size  = 0;
    ret_val = ioctl(file_desc, BLKGETSIZE64, &file_size_in_bytes);

    if (ret_val < 0) {
        cout << "ioctl_myfat_test BLKGETSIZE64 failed: " << ret_val << endl;
                perror("error info is");
        exit(-1);
    }
    else
    {
        cout << "ioctl_myfat_test BLKGETSIZE64 succeeded, sz is " << file_size_in_bytes << endl;
    }

    ret_val = ioctl(file_desc, BLKSSZGET, &blksz);
    if (ret_val < 0) {
        cout << "ioctl_myfat_test BLKSSZGET failed: " << ret_val << endl;
                perror("error info is");
        exit(-1);
    }
    else
    {
        cout << "ioctl_myfat_test BLKSSZGET succeeded, sz is " << blksz << endl;
    }

    ret_val = ioctl(file_desc, BLKGETSIZE, &size);
    if (ret_val < 0) {
        cout << "ioctl_myfat_test BLKGETSIZE failed: " << ret_val << endl;
                perror("error info is");
        exit(-1);
    }
    else
    {
        cout << "ioctl_myfat_test BLKGETSIZE succeeded, sz is " << size << endl;
    }
}




