#include <fstream>
#include <vector>
#include <iostream>
using namespace std;

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

int main()
{
    RandomIPGenerator::initialize();
    for (int i = 0; i < 1000; ++i)
        cout << RandomIPGenerator::getRandomIPstr() << endl;
}