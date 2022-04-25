#include <iostream>
#include <iomanip>
#include <cassert>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/in.h>
#include <netinet/icmp6.h>

#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <signal.h>
#include <errno.h>
#include "config.h"

using namespace std;

int gotalarm = 0;
void sig_alrm(int signo)
{
    cout << "Hello" << endl;
    gotalarm = 1;
}

int main()
{

    char recvbuff[1500];
    sockaddr SA_Recv;
    socklen_t len = 16;
    int recvfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    alarm(1);
    int n = recvfrom(recvfd, recvbuff, sizeof(recvbuff), 0, &SA_Recv, &len);
    
    cout << "Loalll" << endl;

    return 0;
}