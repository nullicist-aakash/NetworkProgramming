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


using namespace std;

#define PAUSE { char c; printf("\nPress any key to continue..."); scanf("%c", &c); system("clear"); }
#define CLEAR_INPUT { char c; while ((c = getchar()) != '\n' && c != '\r' && c != EOF); }
#define PATH_MAX 256
#define MSGQ_PATH "./Shell.cpp"
#define BUF_SIZE 1024


int SignalQueueID;
struct SignalMsg{
    long mtype;
    int signo;
};

int PipeQueueID;
struct PipeMsg{
    long mtype;
    char buf[BUF_SIZE];
};



namespace ShellOperations
{
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

    char** getCommandArguments(char* command, int &size)
    {
        char* c = command;
        char* end = nullptr;
        char delim = ' ';
        size = 0;

        if (!*c)
            return nullptr;

        size++;

        for (char* c = command + 1; *c; end = ++c)
            if (*c == delim && *(c - 1) != delim)
                size++;
        
        end++;

        int argv_index = 0;

        char** result = new  char*[size+1];
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
        result[size] = nullptr;
        size++;
        return result;

    }

     char* getExecutablePath( char* execName)
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
}

namespace ShellOperationTester
{
    void testSplitString()
    {
        char s1[] { "@@abc @@ def ghi @@@@@@@@@@@ aksdha @@" };
        int size = 0;

        cout << "\'" << s1 << "\'" << endl;

        auto res = ShellOperations::splitString(s1, size, '@');

        for (int i = 0; i < size; ++i)
            cout << "\'" << res[i] << "\'" << endl;

        delete[] res;
    }
    void testGetPath()
    {
        auto a = ShellOperations::getExecutablePath("ls");
        cout << a << endl;
        delete[] a;

        cout << (a = ShellOperations::getExecutablePath("wc")) << endl;
        delete[] a;

        cout << (a = ShellOperations::getExecutablePath("./shell.o")) << endl;
        delete[] a;
    }
}

enum class CommandConjunction
{
    MSG_QUEUE,
    SHARED_MEM,
    PIPE,
    INPUT_REDIRECT,
    OUTPUT_REDIRECT,
    OUTPUT_FILE
};

struct Command
{
    int fds[3];
     char* path;
    int argc;
    char** argv;
};

void printCommand(Command c)
{
    cout << "Command : \n" << c.path << endl;
    for (int i = 0; i < c.argc-1; ++i)
        cout << c.argv[i] << ".";
    cout << endl;
}


void removeSpace(char* s)
{
    bool begin = true;
    char* s2 = s;
    do {
        if (*s2 != ' ' && begin)
        {
            begin = false;
            *s++ = *s2;
        }
        else if(!begin)
            *s++ = *s2;
    } while (*s2++ && *s2!='\0');
    *s = '\0';
    *s--;
    while(*s==' ')
        *s--='\0';
}

class Shell
{
    Shell() {}
public:
    Shell(Shell &)        = delete;
    void operator=(Shell &)  = delete;
    static Shell& getInstance()
    { 
        static Shell instance;
        return instance;
    }

    void executeCommands(char* cmd)
    {
        //Get num of commands 
        int pid;
        int ret;
        struct msqid_ds ctl_buf;
        key_t key = ftok(MSGQ_PATH, 'a');

        vector<Command> commands;
        int num_commands = 0;
        char** cmds = ShellOperations::splitString(cmd, num_commands, '|');

        //Creating a pipe
        int pfds[2];
        
        for (int i  = 0; i<num_commands; i++)
        {
            Command c;
            removeSpace(cmds[i]);
            c.argv = ShellOperations::getCommandArguments(cmds[i], c.argc);
            c.path = ShellOperations::getExecutablePath(c.argv[0]);
            commands.push_back(c);
            
        }
        delete[] cmds;

        // for(auto c:commands)
        //     printCommand(c);

        //Create message queue
        // SignalQueueID = msgget(key, IPC_CREAT|IPC_EXCL|0600);
        // if (SignalQueueID == -1)
        // {
        //     perror("msgget:");
        //     exit(1);
        // }
        // ret = msgctl(qid, IPC_STAT, &ctl_buf);
 

        for(int i=0;i<commands.size();i++) 
        {
            int status;
            pipe(pfds);
            pid = fork();
            int numRead;
            char * prev_output_buf[BUF_SIZE];
            if(pid == 0)
            {
                //Child
                if(commands.size()>1 && i!=commands.size()-1 && commands[i].fds[1]!=1)
                {
                    commands[i].fds[1] = pfds[1];
                    cout << commands[i].fds[1] << endl;
                    dup2(commands[i].fds[1], STDOUT_FILENO);
                    close(commands[i].fds[1]);
                }
                if(i>0 &&commands[i].fds[0]!=0)
                {
                    dup2(commands[i].fds[0], STDIN_FILENO);
                    close(commands[i].fds[0]);
                }
                
                //cout << "Execing " <<  i << " " << commands[i].argv[0] << endl;
                if(execvp(commands[i].path, commands[i].argv)<0)
                {
                    perror("execvp");
                    exit(1);
                }
                break;
            }
            else
            {
                close(pfds[1]);
                wait(&status);
                if(i!=commands.size()-1) 
                    commands[i+1].fds[0] = pfds[0];
                delete[] commands[i].path;
                delete[] commands[i].argv;
            }
        }
    }
};

void printPrompt()
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

int main()
{
    loadDFA();
    printPrompt();

    while (true)
    {
        string input;
        std::getline(std::cin, input);

        if(!input.length())
            continue;

        if (input == "exit")
            break;

        Buffer b(input);
        Token* t = getNextToken(b);

        while(t)
        {
            cout << *t << endl;
            t = getNextToken(b);
        }
        
        printPrompt();
    }

    PAUSE;

    return 0;
}
