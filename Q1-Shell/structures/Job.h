#pragma once

#include "../Parsing/AST.h"
#include "Process.h"

class Job
{
private:
    bool isBackground = false;
    bool notified = false;
    bool isDaemon = false;
    int stdin;
    int stdout;
    int stderr;

    void waitForJob();

    void putInForeground(int cont);

    void putInBackground(int cont);

public:
    friend class Shell;
    pid_t pgid = 0;
    Process* firstProcess = nullptr;
    Job* next = nullptr;

    Job(ASTNode* input);

    ~Job();
    bool isStopped() const;
    bool isCompleted() const;
    void launch();
};
