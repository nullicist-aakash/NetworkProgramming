#include "config.h"
#include <vector>
#include <fstream>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int gotalarm = 0;
void sig_alrm(int signo)
{
    gotalarm = 1;
}

int onReceive(int seq, timeval& tv, sockaddr* SA_Recv, int destPORT, int recvfd)
{
    int ret;
    char recvbuff[1500];

    gotalarm = 0;
    alarm(3);

    while (true)
    {
        if (gotalarm)
            return -3;              // timeout
        
        socklen_t len = 16;               // TODO: Remove hardcoded

        int n = recvfrom(recvfd, recvbuff, sizeof(recvbuff), 0, SA_Recv, &len);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            else
            {
                perror("recvfrom error");
                exit(errno);
            }
        }

        auto IPPayload = (ip*)recvbuff;
        int IPHeaderLen1 = IPPayload->ip_hl << 2;
        auto ICMPPayload = (icmp*)(recvbuff + IPHeaderLen1);
        int icmpLen = n - IPHeaderLen1;

        // cout << " (from " << sock_ntop_host(SA_Recv) << ": type = " << (int)ICMPPayload->icmp_type << ", code = " << (int)ICMPPayload->icmp_code << ")" << endl;

        if (icmpLen < 8 + sizeof(ip))
            continue;
            
        auto ICMP_Sent_IP = (ip*)(recvbuff + IPHeaderLen1 + 8);
        int IPHeaderLen2 = ICMP_Sent_IP->ip_hl << 2;

        if (icmpLen < 8 + IPHeaderLen2 + 4)
            continue;

        auto udp = (udphdr*)(recvbuff + IPHeaderLen1 + 8 + IPHeaderLen2);
        
        if (ICMP_Sent_IP->ip_p == IPPROTO_UDP &&
                udp->uh_sport == htons((getpid() & 0xffff) | 0x8000) &&
                udp->uh_dport == htons(destPORT))
        {
            if (ICMPPayload->icmp_type == ICMP_TIMXCEED && ICMPPayload->icmp_code == ICMP_TIMXCEED_INTRANS)
            {
                ret = -2;
                break;
            }
            else if (ICMPPayload->icmp_type == ICMP_UNREACH)
            {
                if (ICMPPayload->icmp_code == ICMP_UNREACH_PORT)
                    ret = -1;
                else
                    ret = ICMPPayload->icmp_code;
                
                break;
            }
        }        
    }

    alarm(0);
    gettimeofday(&tv, nullptr);
    return ret;
}

void setSignalHandler()
{
    struct sigaction new_action { .sa_flags = 0 };
    new_action.sa_handler = sig_alrm;
    sigaction(SIGALRM, &new_action, NULL);
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
            ret.push_back(sa);
    }

    return ret;
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        cerr << "Usage: ./a.out <num_ip_to_generate>" << endl;
        return -1;
    }

    // Get all IP addresses    
    auto sockaddrs = getIPAddresses(atoi(argv[1]));
    ifstream inf {"urls.txt"};

    if (!inf)
    {
        cerr << "Could not open file urls.txt for reading!!" << endl;
    }

    for (auto &x: getFileIPAddresses(inf))
        sockaddrs.push_back(x);

    for (auto &x: sockaddrs)
    {
        char c[16];
        cout << inet_ntop(AF_INET, &x.sin_addr, c, 16) << endl;
    }

    setSignalHandler();

    sockaddr_in *SA_Send = &sockaddrs[0];
    char h[16];
    inet_ntop(AF_INET, &SA_Send->sin_addr, h, 16);

    cout << "Traceroute to " << h << "): " << max_ttl << " hops max, " << ICMPSize << " data bytes" << endl;

    int recvfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (recvfd < 0)
    {
        perror("raw socket error");
        exit(errno);
    }
    
    seteuid(1000);

    int sendfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sendfd < 0)
    {
        perror("simple socket error");
        exit(errno);
    }

    // get unique source port number for ourself to distinguish later
    sockaddr SA_bind;
    SA_bind.sa_family = AF_INET;
    ((sockaddr_in*)&SA_bind)->sin_port = htons((getpid() & 0xffff) | 0x8000);
    cout << "PORT requested: " << ntohs(((sockaddr_in*)&SA_bind)->sin_port) << endl;
    
    int n = bind(sendfd, &SA_bind, 16);
    if (n < 0)
    {
        perror("bind error");
        exit(errno);
    }
    
    sig_alrm(SIGALRM);

    int seq = 0, done = 0;
    for (int ttl = 1; ttl <= max_ttl && !done; ++ttl)
    {
        setsockopt(sendfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(int));

        sockaddr SA_last;

        bzero(&SA_last, sizeof(sockaddr));

        cout << ttl << "\t";
        for (int probe = 0; probe < 3; ++probe)
        {
            ICMPInfo info;
            info.rec_seq = ++seq;
            info.rec_ttl = ttl;
            gettimeofday(&info.rec_tv, nullptr);

            ((sockaddr_in*)SA_Send)->sin_port = htons(32768 + 666 + seq);
            int n = sendto(sendfd, &info, ICMPSize, 0, (sockaddr*)SA_Send, 16);

            if (n < 0)
            {
                perror("sock error");
                exit(errno);
            }

            int code;
            timeval tvrecv;
            sockaddr SA_Recv;

            if ((code = onReceive(seq, tvrecv, &SA_Recv, 32768 + 666 + seq, recvfd)) == -3)
            {
                cout << "\t*";
                fflush(stdout);
                continue;
            }
            
            if (sock_cmp_addr(&SA_Recv, &SA_last, 16) != 0)
            {
                cout << "\t" << sock_ntop_host(&SA_Recv);
                memcpy(&SA_last, &SA_Recv, 16);
            }

            tv_sub(&tvrecv, &info.rec_tv);
            double rtt = tvrecv.tv_sec * 1000.0 + tvrecv.tv_usec / 1000.0;
            cout << "\t" << setprecision(3) << rtt << "ms";

            if (code == -1)
                done++;
            else if (code >= 0)
                cout << "\t (ICMP " << icmpcode_v4(code) << ")";

            fflush(stdout);
        }

        cout << endl;
    }
    return 0;
}
