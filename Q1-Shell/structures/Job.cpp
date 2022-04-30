#include "Job.h"
#include "Shell.h"
#include <sys/wait.h>
#include <unistd.h>


void Job::waitForJob()
{
    int status;
    pid_t pid;
    do
    {
        pid = waitpid(WAIT_ANY, &status, WUNTRACED);
    } while (!Shell::getInstance().markProcessStatus(pid, status) && !this->isStopped() && !this->isCompleted());
}

void Job::putInForeground(int shell_terminal, int shell_pgid, int cont)
{
    tcsetpgrp(shell_terminal, this->pgid);

    waitForJob();
    tcsetpgrp(shell_terminal, shell_pgid);
}

void Job::putInBackground(int cont)
{
    if (cont && kill(-this->pgid, SIGCONT) < 0)
        perror("kill");
}

Job::Job(ASTNode* input)
{
    ASTNode* commands = input->children[0];
    
    if (input->children[0]->token->type == TokenType::TK_TOKEN)
        this->firstProcess = new Process(commands);
    else
    {
        this->firstProcess = new Process(commands->children[0]);
        Process* cur_proc = this->firstProcess;
        ASTNode* cmd = commands->children[0]->sibling;

        while (cmd)
        {
            Process* process = new Process(cmd);
            cur_proc->next = process;
            cur_proc = cur_proc->next;
            cmd = cmd->sibling;
        }
    }

    this->isBackground = input->isBackground;
    stdin = dup(0);
    stdout = dup(1);
    stderr = dup(2);
}

Job::~Job()
{
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

void Job::launch(int shell_terminal)
{
    pid_t pid;
    int pfds[2], infile, outfile;

    infile = this->stdin;
    for (auto p = this->firstProcess; p; p = p->next)
    {
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
            p->launch(shell_terminal, this->pgid, {infile, outfile, this->stderr}, this->isBackground);
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
            int status;
            wait(&status);
        }

        if (infile != this->stdin)
            close(infile);

        if (outfile != this->stdout)
            close(outfile);

        infile = pfds[0];
    }
}
