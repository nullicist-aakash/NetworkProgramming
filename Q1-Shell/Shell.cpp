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
#include <libexplain/setpgid.h>
#include "Lexer.h"
#include "Parser.h"
#include "AST.h"


using namespace std;

#define PAUSE { char c; printf("\nPress any key to continue..."); scanf("%c", &c); system("clear"); }
#define CLEAR_INPUT { char c; while ((c = getchar()) != '\n' && c != '\r' && c != EOF); }
#define PATH_MAX 256
#define MSGQ_PATH "./Shell.cpp"
#define BUF_SIZE 1024

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
}

struct Command
{
    char* path;
    int argc;
    char** argv;
};

class Group
{
    static Group* createGroup(ASTNode* program)
    {
        Group* g = new Group;

        ASTNode* cmds = program->children[0];
        g->commandType = cmds->token->type;
        g->endGroup = nullptr;
        g->isBackground = program->isBackground;
        g->isDaemon = program->isDaemon;

        if (cmds->token->type == TokenType::TK_EXIT || cmds->token->type == TokenType::TK_FG || cmds->token->type == TokenType::TK_BG)
        {
            Command c;
            c.argv = new char*[2];
            
            int sz = cmds->token->lexeme.length() + 1;
            c.argv[0] = new char[sz];
            strcpy(c.argv[0], cmds->token->lexeme.c_str());

            c.argv[1] = nullptr;

            g->commands.push_back(c);
            return g;
        }

        while (cmds)
        {
            if (cmds->token != nullptr && cmds->token->type == TokenType::TK_COMMA)
                break;

            ASTNode* cmd = cmds->children[0];
         
            Command c;
            c.path = ShellOperations::getExecutablePath(cmd->token->lexeme.c_str());
            c.argc = 1;

            while (cmd)
            {
                c.argc++;
                cmd = cmd->sibling;
            }

            c.argv = new char*[c.argc];
            cmd = cmds->children[0];
            int cur_index = 0;

            while (cmd)
            {
                int sz = cmd->token->lexeme.length() + 1;
                c.argv[cur_index] = new char[sz];
                strcpy(c.argv[cur_index], cmd->token->lexeme.c_str());
                cmd = cmd->sibling;
                cur_index++;
            }

            c.argv[c.argc - 1] = nullptr;
            g->commands.push_back(c);
            cmds = cmds->sibling;
        }
    
        return g;
    }

public:
    bool isDaemon = false;
    bool isBackground = false;
    TokenType commandType;
    vector<Command> commands;
    Group* endGroup;

    void fillCommands(ASTNode* program)
    {
        auto group = createGroup(program);
        this->isDaemon = group->isDaemon;
        this->isBackground = group->isBackground;
        this->commandType = group->commandType;
        this->commands = group->commands;
        this->endGroup = nullptr;

        // TODO: Add comma support
    }

    friend std::ostream& operator<<(std::ostream&, const Group&);
};

std::ostream& operator<<(std::ostream& out, const Group& group)
{
    out << "Group: " << endl;
    cout << group.commands.size() << endl;
    for (auto& c : group.commands)
    {
        out << "Command: " << endl;
        out << "Path: " << c.path << endl;
        out << "Argc: " << c.argc << endl;
        out << "Argv: ";
        for (int i = 0; i < c.argc-1; ++i)
            out << c.argv[i] << " ";
        out << endl;
    }
    out << (group.isDaemon ? "Daemon" : "Not a daemon") << endl;
    out << (group.isBackground ? "Background" : "Not a background") << endl;

    return out;
}

class Shell
{
    vector<Group*> back_groups;   // TODO: maybe use set here in future

    Shell() {}

    void _execCommand(const Group& group)
    {
        if (group.commands.size() == 1 && strcmp(group.commands[0].argv[0], "exit") == 0)
            exit(0);

        int temp_input = dup(0);
        int temp_output = dup(1);

        int fdin, fdout;
        fdin = dup(temp_input);

        int ret;
        pid_t firstChildPid;
        int firstChildGid;

        for (int i = 0; i < group.commands.size() ; i++)
        {
            dup2(fdin,0);
            close(fdin);
            if (i == group.commands.size() - 1)
            {
                fdout = dup(temp_output);
            }
            else
            {
                int fdps[2];
                pipe(fdps);
                fdout = fdps[1];
                fdin = fdps[0];
            }

            dup2(fdout, 1);
            close(fdout);

            if ((ret = fork()) == 0)
            {
                if (i != 0)
                {
                    cerr << "Setting group ID of process " << getpid() << " to " << firstChildGid << endl;
                    if (setpgid(0, firstChildGid) < 0)
                    {
                        fprintf(stderr, "%s\n", explain_setpgid(pid, pgid));
                    }
                }
                else
                    setpgid(0, 0);
                
                cerr << i << ": pid - "  << getpid() << ", gid - " << getpgid(0) << endl;
                if (execv(group.commands[i].path, group.commands[i].argv) == -1)
                {
                    perror("execv");
                    exit(1);
                }
                perror("execv error");
                exit(1);
            }
            else
                setpgid(ret, firstChildGid);

            if (!group.isBackground)
            {
                if (i == 0)
                {
                    firstChildGid = ret;
                    cerr << ret << endl;
                }
                wait(NULL);
            }
        }

        dup2(temp_input, 0);
        dup2(temp_output, 1);
        close(temp_input);
        close(temp_output);

        fflush(stdout);
        cout << endl;
    }

public:
    Shell(Shell &)        = delete;
    void operator=(Shell &)  = delete;
    
    static Shell& getInstance()
    { 
        static Shell instance;
        return instance;
    }

    void executeCommand(ASTNode* program)
    {
        Group* g = new Group;
        g->isDaemon = program->isDaemon;
        g->isBackground = program->isBackground;
        g->fillCommands(program);

        _execCommand(*g);

        if (g->isBackground)
            back_groups.push_back(g);
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

int main()
{
    freopen("shell.log","w",stderr);
    loadDFA();
    loadParser();

    while (true)
    {
        cout << endl;
        printPrompt();

        string input;
        std::getline(std::cin, input);

        if (!input.length())
            continue;

        Buffer b(input);
        bool isParseErr;
        auto x = parseInputSourceCode(b, isParseErr);
        
        if (isParseErr)
        {
            cout << "Input command is syntactically incorrect" << endl;
            continue;
        }
        
        ASTNode* ast = createAST(x);
        printAST(ast, 0);

        Shell::getInstance().executeCommand(ast);
    }

    PAUSE;

    return 0;
}
