#pragma once
#include "../Parsing/AST.h"
#include <vector>
#include <sys/types.h>

class Process
{
public:
    Process* next;
    char** argv;
    pid_t pid;
    bool isCompleted;
    bool isStopped;
    int status;

    Process(ASTNode* command);
    void launch(pid_t pgid, std::vector<int> fds, bool isBackground);
};
