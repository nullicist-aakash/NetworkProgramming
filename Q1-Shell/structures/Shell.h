#pragma once
#include "Job.h"
#include <pwd.h>


#define PATH_MAX 256

class Shell
{
public:
    int shell_terminal;
    pid_t shell_pgid;
    Job* first_job = nullptr;

    static Shell& getInstance();

    char** splitString(char* input, int &size, char delim = ' ');

    char* getExecutablePath(const char* execName);

    void executeJob(const char* input);

    void initialize();

    void printPrompt() const;

    Job* findJob(pid_t pgid);

    int markProcessStatus(pid_t pid, int status);
};
