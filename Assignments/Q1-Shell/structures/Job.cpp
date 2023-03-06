#include "Job.h"
#include "Shell.h"
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
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
    
    if (input->isDaemon && input->children[0]->token->type != TokenType::TK_TOKEN)
    {
        cout << "Invalid Command!! No pipe is supported with daemon. Shell will now exit" << endl;
        exit(-1);
    }

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
    this->isDaemon = input->isDaemon;
    stderr = dup(1);

    if (input->children[1])
    {
        auto redirect = input->children[1];
        char *completePath = new char[256];

        // redirect stdio
        if (redirect->children[0])
        {
            string fileLoc = redirect->children[0]->children[0]->token->lexeme;
            realpath(fileLoc.c_str(), completePath);
            FILE* fp = fopen(completePath, "r");
            if (fp == NULL)
            {
                cout << fileLoc << ": " << strerror(errno) << ". Shell will now exit" << endl;
                exit(-1);
            }
            
            stdin = fileno(fp);
        }
        else
        {
            stdin = dup(0);
        }

        // redirect stdout
        if (redirect->children[1])
        {
            string fileLoc = redirect->children[1]->children[0]->token->lexeme;
            realpath(fileLoc.c_str(), completePath);
            FILE* fp;

            if (redirect->children[1]->token->type == TokenType::TK_OUT_NEW_REDIRECT)
                fp = fopen(completePath, "w");
            else
                fp = fopen(completePath, "a");
            
            if (fp == NULL)
            {
                cout << fileLoc << ": " << strerror(errno) << ". Shell will now exit" << endl;
                exit(-1);
            }

            stdout = fileno(fp);          
        }
        else
            stdout = dup(1);

        delete[] completePath;
    }
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

    if (this->isDaemon)
    {
        cout << "Launching daemon" << endl;

        if (fork() != 0)
            return;
        
        if ((pid = fork()) != 0)
            exit(0);
        
        auto input = fdopen(this->stdin, "r");
        freopen("/dev/null", "r", input);
        freopen("daemonStdOut.log", "w", fdopen(this->stdout, "w"));
        freopen("daemonStdErr.log", "w", fdopen(this->stdout, "w"));

        setsid();
        chdir("/");

        umask(0);

        this->firstProcess->launch(this->pgid, { this->stdin, this->stdout, this->stderr }, false);
        
        // no need to call exit
    }

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