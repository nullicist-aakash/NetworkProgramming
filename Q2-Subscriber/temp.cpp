#include <iostream>
#include "helpers/SocketLayer.h"

using namespace std;

int main()
{
    ClientPayload p;
    cout << sizeof(p) << endl;
    cout << ((char*)&p.time - (char*)&p)     << "\t" << sizeof(p.time) << endl;
    cout << ((char*)&p.msgType - (char*)&p)  << "\t" << sizeof(p.msgType) << endl;
    cout << ((char*)&p.topic - (char*)&p)    << "\t" << sizeof(p.topic) << endl;
    cout << ((char*)&p.msg - (char*)&p)      << "\t" << sizeof(p.msg) << endl;
}