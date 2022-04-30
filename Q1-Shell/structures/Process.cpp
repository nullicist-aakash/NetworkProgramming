#include "Process.h"

#include <iostream>
#include <unistd.h>
#include <signal.h>
#include "Shell.h"

using namespace std;

Process::Process(ASTNode* command)
{
    ASTNode* cmd = command->children[0];
    int sz = 1;
    ASTNode* cur = cmd;
    while(cur)
    {
        sz++;
        cur = cur->sibling;
    }
    cerr << "sz = " << sz << endl;
    cur = cmd;
    this->argv = new char*[sz];
    for(int i = 0; i < sz - 1; i++)
    {
        cerr << i << endl;
        int sz = cmd->token->lexeme.length() + 1;
        this->argv[i] = new char[sz];
        strcpy(this->argv[i], cmd->token->lexeme.c_str());
        cmd = cmd->sibling;
    }
    this->argv[sz-1] = nullptr;
    this->next = nullptr;
    this->isCompleted = false;
    this->isStopped = false;
}

void Process::launch(int shell_terminal, pid_t pgid, vector<int> fds, bool isBackground)
{
    pid_t pid = getpid();
    if (pgid == 0)
        pgid = pid;
    setpgid(pid, pgid);
    if(!isBackground)
        tcsetpgrp(shell_terminal, pgid);
    
    vector<int> signals = {SIGINT, SIGQUIT, SIGTSTP, SIGTTIN, SIGTTOU, SIGCHLD};
    for(int sig : signals)
        signal(sig, SIG_DFL);

    for (int i = 0; i < 3; i++)
        if (fds[i] != i)
        {
            dup2(fds[i], i);
            close(fds[i]);
        }
    char* path = Shell::getInstance().getExecutablePath(this->argv[0]);
    if(execv(path, this->argv) < 0)
    {
        perror("execv");
        exit(1);
    }
}
