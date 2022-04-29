#include <cassert>
#include <iostream>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <sys/msg.h>

using namespace std;

int main()
{
    int pid;
    int firstChildPid;
    int firstChildGid;
    for(int i=0;i<10;i++)
    {
        if((pid=fork())==0)
        {
            cout << "bef grp : " << i << " " << tcgetpgrp(getpid()) << endl;
            if(i == 0)
            {
                firstChildPid = getpid();
                firstChildGid = tcgetpgrp(STDIN_FILENO);
                cerr << firstChildPid << " and " << firstChildGid << endl;
            }
            else
                setpgid(STDIN_FILENO, firstChildGid);
            cout << "grp : " << i << " " << tcgetpgrp(STDIN_FILENO) << endl;
            
            break;
        }
        else
        {
            int status;
            waitpid(pid,&status,0);
        }
    }
}