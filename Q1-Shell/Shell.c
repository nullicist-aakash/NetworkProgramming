#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pwd.h>

#define PAUSE { char c; printf("\nPress any key to continue..."); scanf("%c", &c); system("clear"); }
#define CLEAR_INPUT { char c; while ((c = getchar()) != '\n' && c != '\r' && c != EOF); }

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

char** split_string(char* in, int *argc, char delim)
{
	int count = 1;
	int len = strlen(in);

	if (len == 0)
		return NULL;

	for (int i = 1; i < len; ++i)
		if ((in[i] == delim || in[i] == '\0') && in[i - 1] != delim)
			++count;

	if (in[len - 1] == delim)
		--count;

	if (count == 0)
		return NULL;

	char** strings = calloc(count + 1, sizeof(char*));

	int t = 0;
	while (in[t] == delim)
		++t;

	int argv_index = 0;
	
	while (t < len)
	{
		strings[argv_index++] = in + t;
	
		while (t < len && in[t] != delim)
			t++;

		in[t++] = '\0';

		while (t < len && in[t] == delim)
			++t;
	}

	if (argc != NULL)
		*argc = count;

	return strings;
}

char* getExecPath(char* in)
{
	if (in == NULL)
		return NULL;

	// Case 1: When absolute file path is already given
	if (in[0] == '/')
	{
		char* ret = calloc(strlen(in) + 1, sizeof(char));
		strcpy(ret, in);
		return ret;
	}

	// Case 2: When we need to search for location in PATH env variable
	char* tmp = getenv("PATH");
	char* env_path = calloc(strlen(tmp) + 1, sizeof(char));
	strcpy(env_path, tmp);

	int count = 0;
	char** locs = split_string(env_path, &count, ':');

	char complete_path[PATH_MAX + 1];
	
	// Append current file loc to all paths and check if such file exists
	for (int i = 0; i < count; ++i)
	{
		int len = strlen(locs[i]);
		
		strcpy(complete_path, locs[i]);
		complete_path[len] = '/';
		strcpy(complete_path + len + 1, in);

		if (access(complete_path, F_OK | X_OK) != 0)
			continue;

		char* ret = calloc(strlen(complete_path) + 1, sizeof(char));
		strcpy(ret, complete_path);
		free(env_path);
		free(locs);
		return ret;
	}

	// deallocate resources
	free(env_path);
	free(locs);

	// Case 3: Find file w.r.t. relative pos
	realpath(in, complete_path);

	if (access(complete_path, F_OK | X_OK) == 0)
	{
		printf("File found at: %s\n", complete_path);
		char* ret = calloc(strlen(complete_path) + 1, sizeof(char));
		strcpy(ret, complete_path);
		return ret;
	}

	return NULL;
}

int main()
{
	system("clear");
	printf("Shell by Aakash - 2018B4A70887P\n");
	printf("This shell is created as the part of assignment given in Network Programming course\n");
	printf("\n\n");
	
	do 
	{
		// Take input
		printPrompt();
		char input[256];
		memset(input, 0, sizeof(input));

		scanf("%[^\n]", input);
		CLEAR_INPUT;

		// Split the input
		int argc = 0;
		char** argv = NULL;

		argv = split_string(input, &argc, ' ');
		
		if (argv == NULL)
		{
			printf("\n");
			continue;
		}

		printf("{ ");
		for (int i = 0; i < argc; ++i)
			printf("\"%s\", ", argv[i]);
		printf("\b\b }\n");
		
		// get absolute file path of executable
		char* path = getExecPath(argv[0]);

		if (path == NULL)
		{
			printf("File '%s' not found for execution.\n", argv[0]);
			free(argv);
			printf("\n");
			continue;
		}

		printf("File found at: %s\n", path);
		
		
		free(path);
		free(argv);
		printf("\n");

	} while (1);

	PAUSE;
	return 0;
}
