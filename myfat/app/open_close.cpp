
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>


#include <cstdio>

#include <iostream>
#include <string>


int main (int argc, char *argv[])
{
    printf("hello\n");
    std::string str;
    std::cout << "file name: ";
    std::cin >> str;
    
    int fd = open(str.c_str(), O_RDONLY);
    if (-1 == fd)
    {
        printf("open %s failed\n", str.c_str());
    }
    else
    {
        printf("open %s succeeded\n", str.c_str());
    }


    int x = 0;
    std::cin >> x;

    close(fd);

    return 0;
}



