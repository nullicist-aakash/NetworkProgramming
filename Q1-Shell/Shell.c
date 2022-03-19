#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
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

int main()
{
	system("clear");
	printf("Shell by Aakash - 2018B4A70887P\n");
	printf("This shell is created as the part of assignment given in Network Programming course\n");
	printf("\n\n");
	
	do 
	{
		printPrompt();
		char input[256];

		scanf("%[^\n]", input);
		CLEAR_INPUT;
		printf("\n");

	} while (1);

	PAUSE;
	return 0;
}
