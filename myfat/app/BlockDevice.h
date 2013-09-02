#ifndef _BLOCK_DEVICE_H
#define _BLOCK_DEVICE_H

#include <string>
#include <stdexcept>

class BlockDevice
{
public:
    BlockDevice(const std::string &name) throw (std::runtime_error);
    ~BlockDevice() throw ();

    // size of the whole device in byte
    unsigned long long getsize64() throw (std::runtime_error);

    // according to linux source code comment 
    // this is for "get block device sector size"
    // according to blockdev's manual
    // this is for "get logical block (sector) size"
    size_t getss() throw (std::runtime_error);

    // according to blockdev's manual
    // this is for "get physical block (sector) size"
    size_t getpbsz() throw (std::runtime_error);

    // according to blockdev's manual
    // this is for "get blocksize"
    size_t getbsz() throw (std::runtime_error);

    // according to blockdev's manual
    // this is for "get 32-bit sector count"
    size_t getsize() throw (std::runtime_error);

    ssize_t read(void *buf, size_t count);

    ssize_t write(const void *buf, size_t count);
    off_t lset(off_t offset);

private:
    std::string m_name;
    int m_fd;
};

    


#endif


