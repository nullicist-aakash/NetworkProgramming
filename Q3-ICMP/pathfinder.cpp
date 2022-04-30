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

constexpr int max_ttl = 30;                         // Maximum value of TTL to reach from 1 in succession
constexpr int ICMPSize = sizeof(ICMPInfo);          // size of payload
const int num_probes = 3;
int gotalarm = 0;

void sig_alrm(int signo)
{
    gotalarm = 1;
}

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

class DNSResolver
{
    static vector<sockaddr_in> ret;

    enum class IPConvertStatus
    {
        SUCCESS,
        INVALID_ADDR
    };

    static IPConvertStatus convertIPtoN(const char* address, int &ret)
    {
        ret = 0;
        int st = inet_pton(AF_INET, address, &ret);

        if (st == 1)
            return IPConvertStatus::SUCCESS;
        
        return IPConvertStatus::INVALID_ADDR;
    }

public:
    static int getAddr(const char* address, sockaddr_in& addr, bool dnsSend = false)
    {
        bzero(&addr, sizeof(addr));

        int converted;
        auto status = convertIPtoN(address, converted);

        if (status == IPConvertStatus::SUCCESS)
        {
            addr.sin_port = 0;
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = converted;
            return 0;
        }

        if (!dnsSend)
            return -1;

        addrinfo temp;
        addrinfo* info = nullptr;
        bzero(&temp, sizeof(temp));
        temp.ai_flags = AI_CANONNAME;
        if (getaddrinfo(address, 0, &temp, &info) != 0)
        {
            assert(info == nullptr);
            return -1;
        }

        addr = *(sockaddr_in*)info->ai_addr;
        freeaddrinfo(info);
        return 0;
    }
};

struct CommunicationInfo
{
    ICMPInfo* sentInfo;

    ICMPInfo* recvInfo;
    sockaddr recv_sock_addr;
    
    int receive_status;
};

class ICMPService
{
    int ttl = 0;
    int sendfd;
    int recvfd;

    void send_req(int index, int ttl, bool resetSeq = false)
    {
        static int seq = 0;

        if (resetSeq)
            seq = 0;

        auto &send = SA_Send[index];

        for (int i = 0; i < num_probes; ++i)
        {
            ICMPInfo* info = new ICMPInfo;
            info->rec_seq = seq++;
            info->rec_ttl = ttl;
            gettimeofday(&info->rec_tv, nullptr);

            sentSeqs[index].push_back(seq - 1);
            CommunicationInfo commInfo;
            memset(&commInfo, 0, sizeof(commInfo));

            commInfo.sentInfo = info;
            requests.push_back(commInfo);

            send.sin_port = htons(32768 + 666 + seq - 1);
            int n = sendto(sendfd, info, ICMPSize, 0, (sockaddr*)&send, 16);
            
            if (n < 0)
            {
                perror("send error");
                exit(errno);
            }
        }
    }

    static void* Receiver(void* data)
    {
        ICMPService* service = (ICMPService*)data;
        sockaddr s;
        memset(&s, 0, sizeof(s));
        
        char recvbuff[1500];
        gotalarm = 0;
        alarm(3);

        start:
        if (gotalarm)
            return nullptr;
        
        socklen_t len = sizeof(sockaddr_in);
        sockaddr SA_Recv;
        int n = recvfrom(service->recvfd, recvbuff, sizeof(recvbuff), 0, &SA_Recv, &len);
    
        if (n < 0)
        {
            if (errno == EINTR)
                goto start;
            else
            {
                perror("recvfrom error");
                return nullptr;
            }
        }

        auto IPPayload = (ip*)recvbuff;
        int IPHeaderLen1 = IPPayload->ip_hl << 2;
        auto ICMPPayload = (icmp*)(recvbuff + IPHeaderLen1);
        int icmpLen = n - IPHeaderLen1;
        
        if (icmpLen < 8 + sizeof(ip))
            goto start;
            
        auto ICMP_Sent_IP = (ip*)(recvbuff + IPHeaderLen1 + 8);
        int IPHeaderLen2 = ICMP_Sent_IP->ip_hl << 2;

        if (icmpLen < 8 + IPHeaderLen2 + 4)
            goto start;

        auto udp = (udphdr*)(recvbuff + IPHeaderLen1 + 8 + IPHeaderLen2);
        
        int ret = -10;
        if (ICMP_Sent_IP->ip_p == IPPROTO_UDP && 
            udp->uh_sport == htons((getpid() & 0xffff) | 0x8000))
        {
            if (ICMPPayload->icmp_type == ICMP_TIMXCEED && ICMPPayload->icmp_code == ICMP_TIMXCEED_INTRANS)
            {
                ret = -2;
            }
            else if (ICMPPayload->icmp_type == ICMP_UNREACH)
            {
                if (ICMPPayload->icmp_type == ICMP_UNREACH)
                    ret = -1;
                else
                    ret = ICMPPayload->icmp_code;
            }
        }

        if (ret == -10)
            goto start;
        
        // We received a reply
        ICMPInfo* recvInfo = new ICMPInfo;
        recvInfo->rec_seq = ntohs(udp->uh_dport) - (32768 + 666);
        recvInfo->rec_ttl = 0;
        gettimeofday(&recvInfo->rec_tv, nullptr);

        service->requests[recvInfo->rec_seq].recvInfo = recvInfo;
        service->requests[recvInfo->rec_seq].receive_status = ret;
        memcpy(&service->requests[recvInfo->rec_seq].recv_sock_addr, &SA_Recv, sizeof(SA_Recv));
        goto start;
    }

public:
    vector<sockaddr_in> SA_Send;
    map<int, vector<int>> sentSeqs;
    vector<CommunicationInfo> requests;
    set<int> finishedIPs;

    ICMPService()
    {
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
    }

    inline void addRemoteHost(const sockaddr_in& addr)
    {
        SA_Send.push_back(addr);
    }

    void addRemoteHosts(const vector<sockaddr_in> &addrs)
    {
        for (auto &x: addrs)
            SA_Send.push_back(x);
    }

    inline vector<sockaddr_in>& getHosts()
    {
        return SA_Send;
    }

    void sendnextTTLRequests()
    {
        sentSeqs.clear();

        for (auto &x: requests)
        {
            if (x.recvInfo != nullptr)
                delete x.recvInfo;

            if (x.sentInfo != nullptr)
                delete x.sentInfo;
        }

        requests.clear();

        ttl++;
        setsockopt(sendfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(int));

        for (int i = 0; i < SA_Send.size(); ++i)
            if (finishedIPs.find(i) == finishedIPs.end())
                send_req(i, ttl, i == 0);
        
        Receiver(this);
    }

    ~ICMPService()
    {
        for (auto &x: requests)
        {
            if (x.recvInfo != nullptr)
                delete x.recvInfo;

            if (x.sentInfo != nullptr)
                delete x.sentInfo;
        }
    }
};

vector<sockaddr_in> getIPAddresses(int ip_count)
{
    vector<sockaddr_in> temp;

    while (ip_count--)
    {
        sockaddr_in tmp;
        DNSResolver::getAddr(RandomIPGenerator::getRandomIPstr().c_str(), tmp, false);
        temp.push_back(tmp);
    }

    return temp;
}

vector<sockaddr_in> getFileIPAddresses(ifstream &stream)
{
    vector<sockaddr_in> ret;
    while (stream)
    {
        string s;
        stream >> s;
        
        if (s.size() == 0)
            continue;

        sockaddr_in sa;
        if (DNSResolver::getAddr(s.c_str(), sa, true) == -1)
            cerr << "Could not resolve IP for website: " << s <<  endl;
        else
        {
            char buff[20];

            cout << s << "\t" << inet_ntop(AF_INET, &sa.sin_addr, buff, sizeof(buff)) << endl;
            ret.push_back(sa);
        }
    }

    return ret;
}

ICMPService service;

int main(int argc, char** argv)
{
    srand(time(NULL));

    if (argc != 2)
    {
        cerr << "Usage: ./a.out <num_ip_to_generate>" << endl;
        return -1;
    }

    cout << "MAX TTL: " << max_ttl << endl;

    // Get all IP addresses
    cout << "Generating Random IP Addresses" << endl;
    service.addRemoteHosts(getIPAddresses(atoi(argv[1])));
    ifstream inf { "urls.txt" };
    if (!inf)
        cerr << "Could not open file urls.txt for reading!!" << endl;
    
    cout << "Generating IP Addresses from urls in file" << endl;
    service.addRemoteHosts(getFileIPAddresses(inf));
    cout << "IP Address generation completed" << endl;

    struct sigaction new_action { .sa_flags = 0 };
    new_action.sa_handler = sig_alrm;
    sigaction(SIGALRM, &new_action, NULL);
    sig_alrm(SIGALRM);

    vector<int> ips;

    // seteuid(1000);
    for (int i = 0; i < 20; ++i)
    {
        set<uint32_t> addrs;
        cerr << "TTL: " << i + 1 << endl;
        service.sendnextTTLRequests();

        for (int i = 0; i < service.SA_Send.size(); ++i)
        {

            if (service.finishedIPs.find(i) != service.finishedIPs.end())
                continue;

            char buff[100];
            cerr << "For host " << inet_ntop(AF_INET, &service.SA_Send[i].sin_addr, buff, 100) << endl;

            for (auto &x: service.sentSeqs[i])
            {
                auto comm = service.requests[x];
                if (comm.recvInfo == nullptr)
                {
                    cerr << "\t*" << endl;
                    continue;
                }

                tv_sub(&comm.recvInfo->rec_tv, &comm.sentInfo->rec_tv);
                double rtt = comm.recvInfo->rec_tv.tv_sec * 1000.0 + comm.recvInfo->rec_tv.tv_usec / 1000.0;

                cerr << "\t" <<  inet_ntop(AF_INET, &(((sockaddr_in*)(&comm.recv_sock_addr))->sin_addr), buff, 100) << ": " << rtt << "ms" << endl;
            
                if (comm.receive_status == -1)
                    service.finishedIPs.insert(i);
            }
        
            for (auto &x: service.sentSeqs[i])
            {
                auto &addr = service.requests[x].recv_sock_addr;
                auto addr_in = *(sockaddr_in*)&addr;
                uint32_t val = addr_in.sin_addr.s_addr;
                addrs.insert(val);
            }
        }
    
        addrs.erase(0);

        if (ips.size() == i && addrs.size() == 1)
            ips.push_back(*addrs.begin());
    }

    cout << "Common Path: ";

    if (ips.size() == 0)
    {
        cout << "NA" << endl;
    }
    else
    {
        for (auto &ip: ips)
        {
            in_addr addr;
            addr.s_addr = ip;
            char buff[1500];
            cout << inet_ntop(AF_INET, &addr, buff, 1500) << " --> ";
        }

        cout << "*" << endl;
    }

    return 0;
}
