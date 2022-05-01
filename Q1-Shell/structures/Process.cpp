#include "Process.h"
#include "Shell.h"
#include <iostream>
#include <unistd.h>
#include <signal.h>
#include "Shell.h"

using namespace std;

char** splitString(char* input, int &size, char delim)
{
    char* c = input;
    char* end = nullptr;
    size = 0;

    if (!*c)
        return nullptr;

    size++;

    for (char* c = input + 1; *c; end = ++c)
        if (*c == delim && *(c - 1) != delim)
            size++;
    
    end++;

    int argv_index = 0;

    char** result = new  char*[size];

    while (c != end)
    {
        while (c != end && *c == delim && *c)
            ++c;
    
        result[argv_index++] = c;
    
        while (c != end && *c != delim && *c)
            c++;

        *c++ = '\0';
    }

    if (!*result[argv_index - 1])
        size--;

    return result;
}

char* getExecutablePath(const char* execName)
{
    if (!execName)
        return nullptr;

    // Case 1: When absolute file path is already given
    if (*execName == '/')
    {
        char* ret = new char[strlen(execName) + 1];
        strcpy(ret, execName);
        return ret;
    }

    // Case 2: When we need to search for location in PATH env variable
    char* tmp = getenv("PATH");
    char* env_path = new char[strlen(tmp) + 1];
    strcpy(env_path, tmp);

    int count = 0;
    char** locs = splitString(env_path, count, ':');

    char* complete_path = new char[PATH_MAX + 1];
    
    // Append current file loc to all paths and check if such file exists
    for (int i = 0; i < count; ++i)
    {
        int len = strlen(locs[i]);
        
        strcpy(complete_path, locs[i]);
        complete_path[len] = '/';
        strcpy(complete_path + len + 1, execName);

        if (access(complete_path, F_OK | X_OK) != 0)
            continue;

        delete[] env_path;
        delete[] locs;
        return complete_path;
    }

    // deallocate resources
    delete[] env_path;
    delete[] locs;

    // Case 3: Find file w.r.t. relative pos
    realpath(execName, complete_path);

    if (access(complete_path, F_OK | X_OK) == 0)
        return complete_path;

    delete[] complete_path;

    return nullptr;
}

Process::Process(ASTNode* command)
{
    this->next = nullptr;

    ASTNode* cmd = command->children[0];
    int sz = 1;
    
    for (ASTNode* cur = cmd; cur; cur = cur->sibling)
        sz++;
    
    this->argv = new char*[sz];
    for (int i = 0; i < sz - 1; i++)
    {
        int sz = cmd->token->lexeme.length() + 1;
        this->argv[i] = new char[sz];
        strcpy(this->argv[i], cmd->token->lexeme.c_str());
        cmd = cmd->sibling;
    }
    
    this->argv[sz - 1] = nullptr;
    
    this->pid = 0;
    this->isCompleted = false;
    this->isStopped = false;
    this->status = 0;
}

// Preconditions: pgid = 0 means new process group will be created
//                fds = { 0, 1, 2 } for final process that we need
//                isBackground: if set, tcsetgroup will call this
//                no forking here, direct execvp is called
void Process::launch(pid_t pgid, vector<int> fds, bool isBackground)
{
    pid_t pid = getpid();
    pgid = pgid ? pgid : pid;
    setpgid(pid, pgid);

    if (!isBackground)
        tcsetpgrp(Shell::getInstance().shell_terminal, pgid);
    
    vector<int> signals = {SIGINT, SIGQUIT, SIGTSTP, SIGTTIN, SIGTTOU, SIGCHLD};
    for(int sig : signals)
        signal(sig, SIG_DFL);

    for (int i = 0; i < 3; i++)
        if (fds[i] != i)
        {
            dup2(fds[i], i);
            close(fds[i]);
        }

    char* path = getExecutablePath(this->argv[0]);
    
    if (path == 0)
    {
        cout << "Could not find the path for " << this->argv[0] << " to execute" << endl;
        exit(1);
    }

    execv(path, this->argv);
    perror("execv");
    exit(1);
}
