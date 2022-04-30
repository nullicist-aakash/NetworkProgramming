#include "config.h"
#include <vector>
#include <map>
#include <set>
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
            ret.push_back(sa);
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

    // seteuid(1000);
    for (int i = 0; i < 20; ++i)
    {
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
        }
    }


    return 0;
}
