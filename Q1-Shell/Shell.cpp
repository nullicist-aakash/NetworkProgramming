#include <cassert>
#include <iostream>
using namespace std;

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

        char** result = new char*[size];

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
}

int main()
{
    ShellOperationTester::testSplitString();
	cout << "Done" << endl;

    return 0;
}
