#include <cassert>
#include <iostream>
#include <cstring>
#include <unistd.h>
#define PATH_MAX 256
using namespace std;

namespace ShellOperations
{
    const char** splitString(char* input, int &size, char delim = ' ')
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

        const char** result = new const char*[size];

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

    const char* getExecutablePath(const char* execName)
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
        const char* tmp = getenv("PATH");
        char* env_path = new char[strlen(tmp) + 1];
        strcpy(env_path, tmp);

        int count = 0;
        const char** locs = splitString(env_path, count, ':');

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

int main()
{
    ShellOperationTester::testGetPath();
	cout << "Done" << endl;
    
    return 0;
}
