#include "Shell.h"
#include <unistd.h>
#include <signal.h>

#include <iostream>

using namespace std;


Shell& Shell::getInstance()
{
    static Shell instance;
    return instance;
}

char** Shell::splitString(char* input, int &size, char delim)
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

char* Shell::getExecutablePath(const char* execName)
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

    auto job = Job(ast);
    Process * p = job.firstProcess;
    cout << p->argv[0] << endl;
    job.launch(shell_terminal);
}

void Shell::initialize()
{
    shell_terminal = STDIN_FILENO;
    while(tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
        kill(-shell_pgid, SIGTTIN);

    vector<int> signalsToIgnore = { SIGINT, SIGQUIT, SIGTSTP, SIGTTIN, SIGTTOU, SIGCHLD};

    for(int sig : signalsToIgnore);
        // signal(sig, SIG_IGN);

    shell_pgid = getpid();
    if(setpgid(shell_pgid, shell_pgid) < 0)
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
    for (auto j=first_job ; j; j=j->next)
        if (j->pgid == pgid)
            return j;

    return nullptr;
}

int Shell::markProcessStatus(pid_t pid, int status)
{
    Job* j;
    Process* p;

    if (pid > 0)
    {
        for(j = first_job; j; j = j->next)
            for(p = j->firstProcess; p; p = p->next)
                if (p->pid == pid)
                {
                    p->status = status;
                    if (WIFSTOPPED(status))
                        p->isStopped = true;
                    else
                    {
                        p->isCompleted = true;
                        if (WIFSIGNALED(status))
                            fprintf (stderr, "%d: Terminated by signal %d.\n",
                            (int) pid, WTERMSIG (p->status));
                    }
                    return 0;
                }
        return -1;
    }
    else if (pid == 0 || errno == ECHILD)
        return -1;
    else
    {
        perror("waitpid");
        return -1;
    }
}
