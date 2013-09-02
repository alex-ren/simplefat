
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>

#include <errno.h>

#include <cstdio>

#include <iostream>
#include <string>


using std::cout;
using std::cin;
using std::endl;
using std::string;



int main (int argc, char *argv[])
{
    printf("hello\n");
    string str;
    cout << "file name: ";
    cin >> str;
    
    int fd = open(str.c_str(), O_RDONLY|O_DIRECT);
    if (-1 == fd)
    {
        printf("open %s failed\n", str.c_str());
        return -1;
    }
    else
    {
        printf("open %s succeeded\n", str.c_str());
    }

    char szBuffer[2048] = {};
    char *pBuf = reinterpret_cast<char*>(((reinterpret_cast<unsigned long>(szBuffer) >> 9) + 1) << 9);
    int n = 0;
    cout << "Number of characters to read: ";
    cin >> n;
    if (n >= 2048 || n < 0)
    {
        cout << "invalid length" << endl;
        close(fd);
        return 1;
    }

    ssize_t ret = read(fd, static_cast<void *>(pBuf), n);
    if (-1 == ret)
    {
        cout << "errno is " << errno << endl;
        perror("read failed");
    }
    else
    {
        pBuf[n] = '\0';
        cout << "read succeeds" << endl << pBuf;
    }



    int x = 0;
    std::cin >> x;

    close(fd);

    return 0;
}



