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


// Split the input string 'in' on the basis of spaces. returns argv (heap allocated) as NULL if input is empty
char** split_input(char* in, int *argc)
{
	int len = strlen(in);
	*argc = 1;
	char **argv;

	// Step 1:	count the number of arguments
	// 		increment count when current character is space (or \0) but prev character was not space
	for (int i = 1; i < len; ++i)
		if ((in[i] == ' ' || in[i] == '\0') && in[i - 1] != ' ')
			++*argc;

	if (in[len - 1] == ' ')
		--*argc;

	// if empty input
	if (*argc == 0 || len == 0)
		return NULL;

	printf("Received: %d arguments: ", *argc);

	argv = calloc(*argc, sizeof(char*));

	int t = 0;
	while (in[t] == ' ')
		++t;

	int argv_index = 0;
	
	while (t < len)
	{
		argv[argv_index++] = in + t;
	
		while (t < len && in[t] != ' ')
			t++;

		in[t++] = '\0';

		while (t < len && in[t] == ' ')
			++t;
	}

	// just to make sure everything is correctly parsed
	assert(argv_index == *argc);

	printf("{ ");
	for (int i = 0; i < *argc; ++i)
		printf("\"%s\", ", argv[i]);
	printf("\b\b }\n");

	return argv;
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

		scanf("%[^\n]", input);
		CLEAR_INPUT;

		// Split the input
		int argc;
		char** argv;

		argv = split_input(input, &argc);

		if (argv != NULL)
			free(argv);

		// Parse the input
		printf("\n");

	} while (1);

	PAUSE;
	return 0;
}
