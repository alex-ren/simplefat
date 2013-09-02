
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>  // for ioctl

#include <stdlib.h>  // for exit

#include "../example/myfat_fs.h"

#include <cstdio>
#include <iostream>
#include <string>




using std::cout;
using std::cin;
using std::endl;
using std::string;


void ioctl_get_msg(int file_desc);

int main (int argc, char *argv[])
{
    printf("hello\n");
    string str = "testbed/aa.txt";
    cout << "file name: " << str << endl;
    // cin >> str;
    
    int fd = open(str.c_str(), O_RDONLY);
    if (-1 == fd)
    {
        printf("open %s failed\n", str.c_str());
        return -1;
    }
    else
    {
        printf("open %s succeeded\n", str.c_str());
    }
    
    ioctl_get_msg(fd);

//     int x = 0;
//     std::cin >> x;

    close(fd);

    return 0;
}

void ioctl_get_msg(int file_desc)
{
    int ret_val;

    ret_val = ioctl(file_desc, FAT_IOCTL_MYFAT_TEST);

    if (ret_val < 0) {
        cout << "ioctl_myfat_test failed: " << ret_val << endl;
                perror("error info is");
        exit(-1);
    }
    else
    {
        cout << "ioctl_myfat_test succeeded" << endl;
    }

}


