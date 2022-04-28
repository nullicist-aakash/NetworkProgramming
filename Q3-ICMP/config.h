#pragma once
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cstring>

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
#include <signal.h>
#include <errno.h>

using namespace std;

struct ICMPInfo                                     // Information required to send to ICMP packet
{
    u_short rec_seq;
    u_short rec_ttl;
    timeval rec_tv;
};

namespace RandomIPGenerator
{
#include <time.h>
#include <stdlib.h>

    void initialize()
    {
        srand(time(NULL));
    }

    bool isValidPublicIP(uint32_t ip)
    {
        if (ip == 0)
            return false;

        if (ip & 0x0A'00'00'00 == 0x0A'00'00'00)    // class A private
            return false;
        
        if (ip & 0xAC'10'00'00 == 0xAC'10'00'00)    // class B private
            return false;

        if (ip & 0xC0'A8'00'00 == 0xC0'A8'00'00)    // class C private
            return false;
        
        if ((ip >> 24) > 223)                       // outside class A/B/C
            return false;
        
        return true;
    }

    uint32_t getNextRandom(uint32_t mod)
    {
        return rand() % mod;
    }

    uint32_t getRandomIP()
    {
        uint32_t ip = 0;
        
        while (!isValidPublicIP(ip))
        {
            int a = getNextRandom(256);
            int b = getNextRandom(256);
            int c = getNextRandom(256);
            int d = getNextRandom(256);

            ip = a | (b << 8) | (c << 16) | (d << 24);
        }

        return ip;
    }

    string getRandomIPstr()
    {
        auto ip = getRandomIP();

        string s = 
            to_string((ip >> 24) & 0xff) + "." +
            to_string((ip >> 16) & 0xff) + "." +
            to_string((ip >> 8) & 0xff) + "." +
            to_string((ip) & 0xff);

        return s; 
    }
}

class Address
{
    const char* hostname;
    const char* servType;
    addrinfo temp;
    addrinfo* info;
    char* hostIP;

public:
    Address(const char* hostname, const char* serviceType = nullptr, int sockFamily = 0, int sockectType = 0) : servType { serviceType }, hostname {hostname}
    {
        bzero(&temp, sizeof(temp));
        temp.ai_flags = AI_CANONNAME;
        temp.ai_family = sockFamily;
        temp.ai_socktype = sockectType;

        info = nullptr;
        hostIP = new char[128];
        hostIP[0] = 0;
    }

    const addrinfo* getHostAddrInfo()
    {
        if (!info && getaddrinfo(hostname, servType, &temp, &info) != 0)
            info = nullptr;

        return info;
    }

    const char* getHostIP()
    {
        auto sa = (sockaddr_in*)info->ai_addr;
        auto x =  &sa->sin_addr;
        if (!hostIP[0] && info && inet_ntop(AF_INET, x, hostIP, 128) == NULL)
        {
            hostIP[0] = 0;
            return nullptr;
        }

        return hostIP;
    }

    ~Address()
    {
        freeaddrinfo(info);
        delete[] hostIP;
    }
};

constexpr int max_ttl = 30;                         // Maximum value of TTL to reach from 1 in succession
constexpr int ICMPSize = sizeof(ICMPInfo);          // size of payload

void tv_sub(struct timeval *out, struct timeval *in)
{
    if ((out->tv_usec -= in->tv_usec) < 0) 
    {
        --out->tv_sec;
        out->tv_usec += 1000000;
    }
    out->tv_sec -= in->tv_sec;
}

int sock_cmp_addr(sockaddr *sa1, sockaddr *sa2, socklen_t salen)
{
	if (sa1->sa_family != sa2->sa_family)
		return -1;

    auto &lhs = ((sockaddr_in*)sa1)->sin_addr;
    auto &rhs = ((sockaddr_in*)sa2)->sin_addr;
    return memcmp(&lhs, &rhs, sizeof(in_addr));
}

char* sock_ntop_host(sockaddr *sa)
{
    static char str[128];
    struct sockaddr_in	*sin = (struct sockaddr_in *) sa;

    if (inet_ntop(AF_INET, &sin->sin_addr, str, sizeof(str)) == NULL)
        return nullptr;

    return str;
}

const char * icmpcode_v4(int code)
{
	switch (code) {
	case  0:	return("network unreachable");
	case  1:	return("host unreachable");
	case  2:	return("protocol unreachable");
	case  3:	return("port unreachable");
	case  4:	return("fragmentation required but DF bit set");
	case  5:	return("source route failed");
	case  6:	return("destination network unknown");
	case  7:	return("destination host unknown");
	case  8:	return("source host isolated (obsolete)");
	case  9:	return("destination network administratively prohibited");
	case 10:	return("destination host administratively prohibited");
	case 11:	return("network unreachable for TOS");
	case 12:	return("host unreachable for TOS");
	case 13:	return("communication administratively prohibited by filtering");
	case 14:	return("host recedence violation");
	case 15:	return("precedence cutoff in effect");
	default:	return("[unknown code]");
	}
}