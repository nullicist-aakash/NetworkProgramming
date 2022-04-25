#include "config.h"

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

int main()
{
    setSignalHandler();

    Address a("www.google.com");
    sockaddr *SA_Send = a.getHostAddrInfo()->ai_addr;

    auto ai = a.getHostAddrInfo();
    auto h = a.getHostIP();
    cout << "Traceroute to " << (ai->ai_canonname ? ai->ai_canonname : h) << " (" << h << "): " << max_ttl << " hops max, " << ICMPSize << " data bytes" << endl;

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

        bzero(&SA_last, sizeof(ai->ai_addrlen));

        cout << ttl << "\t";
        for (int probe = 0; probe < 3; ++probe)
        {
            ICMPInfo info;
            info.rec_seq = ++seq;
            info.rec_ttl = ttl;
            gettimeofday(&info.rec_tv, nullptr);

            ((sockaddr_in*)SA_Send)->sin_port = htons(32768 + 666 + seq);
            int n = sendto(sendfd, &info, ICMPSize, 0, SA_Send, ai->ai_addrlen);

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
            
            if (sock_cmp_addr(&SA_Recv, &SA_last, ai->ai_addrlen) != 0)
            {
                cout << "\t" << sock_ntop_host(&SA_Recv);
                memcpy(&SA_last, &SA_Recv, ai->ai_addrlen);
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
