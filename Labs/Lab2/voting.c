#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

typedef struct my_msgbuf
{
	long mtype;
	int msg;
} my_msgbuf;

int queueID;

int createQueue()
{
	key_t queueKey = ftok(".", 'a');	// get the key for IPC
	
	if (queueKey == -1)
	{
		perror("ftok");
		exit(-1);
	}

	int id = msgget(queueKey, IPC_CREAT | 0660);	// get queue ID
	
	if (queueID == -1)
	{
		perror("msgget");
		exit(-1);
	}

	return id;
}

int childTask(int childID)
{
	srand(time(0) + childID + childID);
	while (1)
	{
		my_msgbuf msg;

		int res = msgrcv(queueID, &msg, sizeof(my_msgbuf) - sizeof(long), getpid(), 0);
		if (res == -1)
		{
			perror("child received msg from parent");
			exit(-1);
		}

		// time to vote

		msg.mtype = getppid();
		msg.msg = rand() % 2;
		printf("Child %2d\t: Voted %d\n", childID, msg.msg);

		res = msgsnd(queueID, &msg, sizeof(my_msgbuf) - sizeof(long), 0);
		if (res == -1)
		{
			perror("sending vote to parent");
			exit(-1);
		}
	}

	return 0;
}

int parentTask(int N, pid_t* pids)
{
	while (1)
	{
		printf("Parent\t\t: Collecting votes\n");
		
		for (int i = 0; i < N; ++i)
		{
			my_msgbuf msg;
			msg.mtype = pids[i];

			int res = msgsnd(queueID, &msg, sizeof(my_msgbuf) - sizeof(long), 0);

			if (res == -1)
			{
				perror("parent sending msg to queue");
				exit(-1);
			}
		}

		int voteCount = 0;
		my_msgbuf received;
		for (int i = 0; i < N; ++i)
		{
			int res = msgrcv(queueID, &received, sizeof(my_msgbuf) - sizeof(long), getpid(), 0);

			if (res == -1)
			{
				perror("receiving msg from child");
				exit(-1);
			}

			printf("Parent\t\t: Received vote %d (%d of %d)\n", received.msg, i + 1, N);
			voteCount += received.msg != 0;
		}

		printf("Parent\t\t: Decision - ");
		if (2 * voteCount == N)
			printf("tie");
		else if (2 * voteCount < N)
			printf("reject");
		else
			printf("accept");

		printf(" (%d/%d)\n", voteCount, N);

		char c;
		do {
			printf("Press q to quit or r to revote: ");
		} while ((c = getchar()) != 'q' && c != 'r');

		if (c == 'q')
			break;

		while ((c = getchar()) != '\n' && c != EOF);
	}
}

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		printf("Invalid number of arguments!!\n");
		printf("<filename> <number of child processes>\n");
		exit(-1);
	}

	int N = atoi(argv[1]);
	queueID = createQueue();

	pid_t* pids = (pid_t*)calloc(N, sizeof(pid_t));

	for (int i = 0; i < N; ++i)
		if ((pids[i] = fork()) == 0)
			return childTask(i + 1);

	printf("Press any key to start...");
	getchar();

	parentTask(N, pids);

	// kill all children
	for (int i = 0; i < N; ++i)
		kill(pids[i], SIGKILL);

	for (int i = 0; i < N; ++i)
		wait(NULL);

	// close the queue
	msgctl(queueID, IPC_RMID, NULL);
}
