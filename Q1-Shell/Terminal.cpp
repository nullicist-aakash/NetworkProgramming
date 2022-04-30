#include <cassert>
#include <iostream>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include "structures/Shell.h"

using namespace std;

#define PAUSE { char c; printf("\nPress any key to continue..."); scanf("%c", &c); system("clear"); }
#define CLEAR_INPUT { char c; while ((c = getchar()) != '\n' && c != '\r' && c != EOF); }
#define BUF_SIZE 1024

int main()
{
    Shell::getInstance().initialize();
    freopen("/dev/null", "w", stderr);
    loadDFA();
    loadParser();

    while (true)
    {
        cout << endl;
        Shell::getInstance().printPrompt();

        string input;
        std::getline(std::cin, input);

        if (!input.length())
            continue;

        Shell::getInstance().executeJob(input.c_str());
    }

    PAUSE;

    return 0;
}