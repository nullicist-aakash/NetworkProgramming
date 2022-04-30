#include <cassert>
#include <iostream>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <sys/msg.h>
#include "Lexer.h"
#include "Parser.h"
#include "AST.h"


using namespace std;

#define PAUSE { char c; printf("\nPress any key to continue..."); scanf("%c", &c); system("clear"); }
#define CLEAR_INPUT { char c; while ((c = getchar()) != '\n' && c != '\r' && c != EOF); }
#define PATH_MAX 256
#define MSGQ_PATH "./Shell.cpp"
#define BUF_SIZE 1024

void printTabs(int cnt)
{
    for (int i = 0; i < cnt; i++)
        cout << "\t";
}

void printAST(ASTNode* node, int tabcnt)
{
    if (node == nullptr)
        return;

    printTabs(tabcnt);
    
    cout << *node << endl;
    
    for (auto child : node->children)
        printAST(child, tabcnt + 2);
    
    if (node->sibling)
        printAST(node->sibling, tabcnt + 1);
}

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
    void launch(int shell_terminal, pid_t pgid, vector<int> fds, bool isBackground);
};

class Job{
private:
    char* command;
    ASTNode* node;
    bool isBackground;
    bool notified;
    int stdin;
    int stdout;
    int stderr;

    void waitForJob();
    void putInForeground(int shell_terminal, int shell_pgid, int cont);

    void putInBackground(int cont);

public:
    
    pid_t pgid;
    Process* firstProcess;
    Job* next;

    Job(ASTNode* input);

    ~Job();
    bool isStopped() const;
    bool isCompleted() const;
    void launch(int shell_terminal);


};

class Shell{
public:
    int shell_terminal;
    pid_t shell_pgid;
    Job* first_job = nullptr;

    static Shell& getInstance()
    {
        static Shell instance;
        return instance;
    }

    char** splitString(char* input, int &size, char delim = ' ')
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

    void executeJob(const char* input)
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
        printAST(ast,0);
        auto job = Job(ast);
        Process * p = job.firstProcess;
        cout << p->argv[0] << endl;
        job.launch(shell_terminal);
    }

    void initialize()
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

    void printPrompt() const
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

    Job* findJob(pid_t pgid)
    {
        for (auto j=first_job ; j; j=j->next)
            if (j->pgid == pgid)
                return j;

        return nullptr;
    }

    int markProcessStatus(pid_t pid, int status)
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


};

Process::Process(ASTNode* command)
{
    ASTNode* cmd = command->children[0];
    int sz = 1;
    ASTNode* cur = cmd;
    while(cur)
    {
        sz++;
        cur = cur->sibling;
    }
    cout << "sz = " << sz << endl;
    cur = cmd;
    this->argv = new char*[sz];
    for(int i = 0; i < sz - 1; i++)
    {
        cout << i << endl;
        int sz = cmd->token->lexeme.length() + 1;
        this->argv[i] = new char[sz];
        strcpy(this->argv[i], cmd->token->lexeme.c_str());
        cmd = cmd->sibling;
    }
    this->argv[sz-1] = nullptr;
    this->next = nullptr;
    this->isCompleted = false;
    this->isStopped = false;
}
    

void Process::launch(int shell_terminal, pid_t pgid, vector<int> fds, bool isBackground)
{
    pid_t pid = getpid();
    if (pgid == 0)
        pgid = pid;
    setpgid(pid, pgid);
    if(!isBackground)
        tcsetpgrp(shell_terminal, pgid);
    
    vector<int> signals = {SIGINT, SIGQUIT, SIGTSTP, SIGTTIN, SIGTTOU, SIGCHLD};
    for(int sig : signals)
        signal(sig, SIG_DFL);

    for (int i = 0; i < 3; i++)
        if (fds[i] != i)
        {
            dup2(fds[i], i);
            close(fds[i]);
        }
    char* path = Shell::getInstance().getExecutablePath(this->argv[0]);
    if(execv(path, this->argv) < 0)
    {
        perror("execv");
        exit(1);
    }
}

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
    if (cont)
    {
        if (kill(-this->pgid, SIGCONT) < 0)
            perror("kill");
    }
}

Job::Job(ASTNode* input)
{
    ASTNode* commands = input->children[0];
    //handle fg , bg , exit
    //If not pipe
    if(input->children[0]->token->type == TokenType::TK_TOKEN)
        this->firstProcess = new Process(commands);
    // if pipe
    else
    {
        this->firstProcess = new Process(commands->children[0]);
        Process* cur_proc = this->firstProcess;
        ASTNode* cmd = commands->children[0]->sibling;
        while(cmd)
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
        if(!p->isCompleted && !p->isStopped)
            return false;
    return true;
}

bool Job::isCompleted() const
{
    for(auto p = this->firstProcess; p; p = p->next)
        if(!p->isCompleted)
            return false;
    return true;
}

void Job::launch(int shell_terminal)
{
    pid_t pid;
    int pfds[2], infile, outfile;

    infile = this->stdin;
    for(auto p = this->firstProcess; p; p = p->next)
    {
        cout << "Hmm\n";
        if(!p->next)
            cout << "Not\n";
        else cout << ".\n";
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
            if(!this->pgid)
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


int main()
{
    Shell::getInstance().initialize();

    freopen("shell.log","w",stderr);
    loadDFA();
    loadParser();


    while (true)
    {
        cout << endl;
        Shell::getInstance().printPrompt();

        string input;
        std::getline(std::cin, input);

        if (!input.length())
            continue;

        Shell::getInstance().executeJob(input.c_str());

        
    }

    PAUSE;

    return 0;
}
