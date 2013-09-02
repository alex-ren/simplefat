
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
    string oldpath, newpath;
    cout << "please input old path: ";
    cin >> oldpath;
    
    cout << "please input new path: ";
    cin >> newpath;

    int ret = rename(oldpath.c_str(), newpath.c_str());

    if (-1 == ret)
    {
        cout << "errno is " << errno << endl;
        perror("rename failed");
    }
    else
    {
        cout << "rename succeeds" << endl;
    }

    return ret;
}
        
