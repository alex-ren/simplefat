#include "BlockDevice.h"


#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <string.h>


#include <linux/fs.h>


#include <cstdio>
#include <cerrno>
#include <iostream>


using std::string;
using std::cerr;
using std::endl;
using std::runtime_error;

BlockDevice::BlockDevice(const std::string &name) throw (std::runtime_error): 
  m_name(name), m_fd(0)
{
    m_fd = open(m_name.c_str(), O_RDWR);
    if (-1 == m_fd)
    {
        cerr << "open file " << m_name << " failed, errno is : " << errno << ", error info is: " <<
          strerror(errno) << endl;
        throw runtime_error("open file " + m_name + " failed");
    }
}

BlockDevice::~BlockDevice() throw ()
{
    if (0 < m_fd)
    {
        close(m_fd);
    }
    m_fd = 0;
}


unsigned long long BlockDevice::getsize64() throw (std::runtime_error)
{
    unsigned long long sz = 0;
    int ret = ioctl(m_fd, BLKGETSIZE64, &sz);
    if (ret < 0)
    {
        cerr << "BLKGETSIZE64 failed, errno is " << errno << " , info is: " << 
            strerror(errno) << endl;
        throw runtime_error("BLKGETSIZE64 failed");
    }
    return sz;
}

size_t BlockDevice::getss() throw (std::runtime_error)
{
    size_t sz = 0;
    int ret = ioctl(m_fd, BLKSSZGET, &sz);
    if (ret < 0)
    {
        cerr << "BLKSSZGET failed, errno is " << errno << " , info is: " << 
            strerror(errno) << endl;
        throw runtime_error("BLKSSZGET failed");
    }
    return sz;
}

size_t BlockDevice::getpbsz() throw (std::runtime_error)
{
    size_t sz = 0;
    int ret = ioctl(m_fd, BLKPBSZGET, &sz);
    if (ret < 0)
    {
        cerr << "BLKPBSZGET failed, errno is " << errno << " , info is: " << 
            strerror(errno) << endl;
        throw runtime_error("BLKPBSZGET failed");
    }
    return sz;
}

size_t BlockDevice::getbsz() throw (std::runtime_error)
{
    size_t sz = 0;
    int ret = ioctl(m_fd, BLKBSZGET, &sz);
    if (ret < 0)
    {
        cerr << "BLKBSZGET failed, errno is " << errno << " , info is: " << 
            strerror(errno) << endl;
        throw runtime_error("BLKBSZGET failed");
    }
    return sz;
}

size_t BlockDevice::getsize() throw (std::runtime_error)
{
    size_t sz = 0;
    int ret = ioctl(m_fd, BLKGETSIZE, &sz);
    if (ret < 0)
    {
        cerr << "BLKBGETSIZE failed, errno is " << errno << " , info is: " << 
            strerror(errno) << endl;
        throw runtime_error("BLKGETSIZE failed");
    }
    return sz;
}

ssize_t BlockDevice::read(void *buf, size_t count)
{
    return ::read(m_fd, buf, count);
}

ssize_t BlockDevice::write(const void *buf, size_t count)
{
    return ::write(m_fd, buf, count);
}
    
off_t BlockDevice::lset(off_t offset)
{
    return ::lseek(m_fd, offset, SEEK_SET);
}



