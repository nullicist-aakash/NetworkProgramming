#include <iostream>
#include <iomanip>
#include <cassert>
#include <cstring>
#include <vector>
#include <map>
#include <set>
#include <fstream>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/in.h>

#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

using namespace std;

int main(int argc, char** argv)
{
	int recvfd, sendfd;
	if ((recvfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0)
	{
	    perror("raw socket error");
	    exit(errno);
	}

	if ((sendfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
	    perror("socket create error for sending");
	    exit(errno);
	}

	sockaddr SA_bind;
	SA_bind.sa_family = AF_INET;
	((sockaddr_in*)&SA_bind)->sin_port = htons((getpid() & 0xffff) | 0x8000);

	cerr << "PORT requested: " << ntohs(((sockaddr_in*)&SA_bind)->sin_port) << endl;

	if (bind(sendfd, &SA_bind, sizeof(SA_bind)) < 0)
	{
	    perror("bind error");
	    exit(errno);
	}

    return 0;
}
