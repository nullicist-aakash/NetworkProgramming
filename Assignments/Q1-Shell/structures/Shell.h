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

    void executeJob(const char* input);

    void initialize();

    void printPrompt() const;

    Job* findJob(pid_t pgid);
};
