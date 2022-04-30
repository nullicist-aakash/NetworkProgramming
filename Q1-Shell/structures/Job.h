#pragma once

#include "../Parsing/AST.h"
#include "Process.h"

class Job
{
private:
    bool isBackground;
    bool notified;
    int stdin;
    int stdout;
    int stderr;

    void waitForJob();

    void putInForeground(int cont);

    void putInBackground(int cont);

public:
    pid_t pgid;
    Process* firstProcess;
    Job* next;

    Job(ASTNode* input);

    ~Job();
    bool isStopped() const;
    bool isCompleted() const;
    void launch();
};
