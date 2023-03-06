#include "Shell.h"
#include <unistd.h>
#include <signal.h>

#include <iostream>

using namespace std;

void printAST(ASTNode* node, int tabs = 0)
{
    if (!node)
        return;
        
    for (int i = 0; i < tabs; ++i)
        cerr << "\t";
    
    cerr << node << " " << *node << endl;

    for (auto &c: node->children)
        printAST(c, tabs + 1);
    
    printAST(node->sibling, tabs + 1);
}

Shell& Shell::getInstance()
{
    static Shell instance;
    return instance;
}

void Shell::executeJob(const char* input)
{
    Buffer b(input);
    bool isParseErr;
    auto x = parseInputSourceCode(b, isParseErr);
    
    if (isParseErr)
    {
        cout << "Input command is syntactically incorrect" << endl;
        return;
    }
    
    ASTNode* ast = createAST(x);
    printAST(ast);

    auto job = Job(ast);
    job.launch();
}

void Shell::initialize()
{
    first_job = nullptr;

    shell_terminal = STDIN_FILENO;
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
        kill(-shell_pgid, SIGTTIN);

    vector<int> signalsToIgnore = { SIGINT, SIGQUIT, SIGTSTP, SIGTTIN, SIGTTOU, SIGCHLD};

    for(int sig : signalsToIgnore)
        signal(sig, SIG_IGN);

    shell_pgid = getpid();
    if (setpgid(shell_pgid, shell_pgid) < 0)
    {
        perror("Error in shell process group creation");
        exit(1);
    }

    tcsetpgrp(shell_terminal, shell_pgid);
}

void Shell::printPrompt() const
{
    char path[PATH_MAX];

    if (getcwd(path, PATH_MAX) == NULL)
    {
        perror("getcwd() error");
        exit(-1);
    }


    struct passwd *pw = getpwuid(geteuid());
    if (pw == NULL)
    {
        printf("getpwuid() error\n");
        exit(-1);
    }

    char user[256];
    strcpy(user, pw->pw_name);

    char hostname[256];
    if (gethostname(hostname, 256) == -1)
    {
        perror("gethostname() error");
        exit(-1);
    }

    // green color
    printf("\033[0;32m");

    // blue color
    printf("--(\033[0;34m");
    printf("%s@ %s", user, hostname);
    
    // green color
    printf("\033[0;32m");
    printf(")-[");

    // white color
    printf("\033[0;37m");
    printf("%s", path);

    // green color
    printf("\033[0;32m");
    printf("]\n--$");

    // white color
    printf("\033[0;37m");
    printf(": ");
    
    // reset color
    printf("\033[0m");
}

Job* Shell::findJob(pid_t pgid)
{
    for (auto j = first_job; j; j = j->next)
        if (j->pgid == pgid)
            return j;

    return nullptr;
}