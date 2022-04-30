#include "Job.h"
#include "Shell.h"
#include <sys/wait.h>
#include <unistd.h>
#include <iostream>
using namespace std;

// TODO: Add update_status

int markProcessStatus(pid_t pid, int status)
{
    if (pid == 0 || errno == ECHILD)
        return -1;
    
    if (pid < 0)
    {
        perror("waitpid");
        return -1;
    }

    for (auto j = Shell::getInstance().first_job; j; j = j->next)
        for(auto p = j->firstProcess; p; p = p->next)
        {
            if (p->pid != pid)
                continue;            

            p->status = status;

            if (WIFSTOPPED(status))
                p->isStopped = true;
            else
            {
                p->isCompleted = true;

                if (WIFSIGNALED(status))
                    cerr << pid << ": Terminated by signal " << WTERMSIG (p->status) << "." << endl;
            }
            return 0;
        }

    return -1;
}

void Job::waitForJob()
{
    int status;
    pid_t pid;
    do
    {
        pid = waitpid(WAIT_ANY, &status, WUNTRACED);
    } while (!markProcessStatus(pid, status) && !this->isStopped() && !this->isCompleted());
}

void Job::putInForeground(int cont)
{
    tcsetpgrp(Shell::getInstance().shell_terminal, this->pgid);
    
    if (cont && kill(-this->pgid, SIGCONT) < 0)
        perror("kill (SIGCONT)");
    
    this->waitForJob();
    tcsetpgrp(Shell::getInstance().shell_terminal, Shell::getInstance().shell_pgid);
}

void Job::putInBackground(int cont)
{
    if (cont && kill(-this->pgid, SIGCONT) < 0)
        perror("kill (SIGCONT)");
}

Job::Job(ASTNode* input)
{
    ASTNode* commands = input->children[0];
    
    if (input->children[0]->token->type == TokenType::TK_TOKEN)
        this->firstProcess = new Process(commands);
    else if (input->children[0]->token->type == TokenType::TK_EXIT)
        exit(0);
    else
    {
        this->firstProcess = new Process(commands->children[0]);
        Process* p = this->firstProcess;
        
        for (ASTNode* cmd = commands->children[0]->sibling; cmd; cmd = cmd->sibling)
        {
            p->next = new Process(cmd);
            p = p->next;
        }
    }

    this->isBackground = input->isBackground;
    this->notified = 0;
    stdin = dup(0);
    stdout = dup(1);
    stderr = dup(2);
    pgid = 0;
    next = nullptr;
}

Job::~Job()
{
    while (firstProcess)
    {
        auto temp = firstProcess;
        firstProcess = firstProcess->next;
        delete temp;
    }

    close(stdin);
    close(stdout);
    close(stderr);
}

bool Job::isStopped() const
{
    for (auto p = this->firstProcess; p; p = p->next)
        if (!p->isCompleted && !p->isStopped)
            return false;

    return true;
}

bool Job::isCompleted() const
{
    for (auto p = this->firstProcess; p; p = p->next)
        if (!p->isCompleted)
            return false;

    return true;
}

void Job::launch()
{
    pid_t pid;
    int pfds[2], infile, outfile;

    infile = this->stdin;
    for (auto p = this->firstProcess; p; p = p->next)
    {
        //  Setup pipes
        if (p->next)
        {
            if (pipe(pfds) < 0)
            {
                perror("pipe");
                exit(1);
            }
            outfile = pfds[1];
        }
        else
            outfile = this->stdout;

        pid = fork();

        if (pid == 0)
            p->launch(this->pgid, {infile, outfile, this->stderr}, this->isBackground);
        else if (pid < 0)
        {
            perror("fork");
            exit(1);
        }
        else
        {
            p->pid = pid;
            
            if (!this->pgid)
                this->pgid = pid;
        
            setpgid(pid, this->pgid);
        }

        if (infile != this->stdin)
            close(infile);

        if (outfile != this->stdout)
            close(outfile);

        infile = pfds[0];
        
        if (isBackground)
            this->putInBackground(0);
        else
            this->putInForeground(0);
    }
}