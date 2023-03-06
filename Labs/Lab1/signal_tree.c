#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

int N, A, S, points;

void handler(int sig, siginfo_t *siginfo, void *ucontext)
{
	// Extract the information about sender
	pid_t sender_pid = siginfo->si_pid;
	pid_t sender_ppid = siginfo->si_value.sival_int;
	
	int new_points = points;
	char* proc_relation;
	
	if (sender_pid == getppid())
	{
		proc_relation = "parent";
		new_points += A;
	}
	else if (sender_ppid == getppid() && sender_pid != getpid())
	{
		proc_relation = "sibling";
		new_points -= (S / 2);
	}
	else if (sender_ppid == getpid())
	{
		proc_relation = "child";
		new_points -= S;
	}
	else if (sender_pid == getpid())
		proc_relation = "self";
	else
		proc_relation = "arbitrary process";
	
	printf("%d\t-\t%3d to %3d\tReceived signal from %d (%s)\n", getpid(), points, new_points, sender_pid, proc_relation);
	points = new_points;

	if (points == 0)
	{
		printf("%d\t-\t\t\tExiting...\n", getpid());

		// It is not a good design but wait is kept here so that we don't have the problem with children attached to init process
		wait(NULL);
		wait(NULL);
		exit(0);
	}
}

void create_processes(int N)
{
	// Create tree with the property that child indices are 
	// { 2i, 2i + 1 } if current index is i. (Assuming 1 as the index of root)
	int cur_index = 1;
	while (1)
	{
		printf("%d is parent of %d\n", getppid(), getpid());
		
		pid_t left;
		if (2 * cur_index > N)
			break;

		left = fork();
		if (left == 0)
		{
			cur_index *= 2;
			continue;
		}
		
		pid_t right;

		if (2 * cur_index + 1 > N)
			break;

		right = fork();
		if (right == 0)
		{
			cur_index = 2 * cur_index + 1;
			continue;
		}
		
		break;
	}
}

int main(int argc, char** argv)
{
	pid_t head = getppid();
	// Argument count check
	if (argc != 4)
	{
		fprintf(stderr, "Invalid number of arguments\n");
		exit(EXIT_FAILURE);
	}

	// Register handler
	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = handler;
	sa.sa_flags = SA_SIGINFO | SA_RESTART;
	sigaction(SIGUSR1, &sa, NULL);
	
	// Extract arguments
	N = atoi(argv[1]);
	A = atoi(argv[2]);
	S = atoi(argv[3]);
	points = N;

	// Create N+1 processes as specified, (1 current and N children)
	create_processes(N + 1);

	// 1 is the magic number here, just waiting for all processes to be spawned in memory
	sleep(1);

	if (getppid() == head)
		printf("PID\t-\t   Points\tMessage\n");

	// Send the signal to required processes
	for (pid_t p = getppid() - N; p <= getpid() + N; ++p)
	{
		// prepare data
		union sigval sv;
		sv.sival_int = getppid();
		
		// only send the signal when there is a process to accept it and don't send the signal to terminal
		if (kill(p, 0) == 0 && p != head)
			sigqueue(p, SIGUSR1, sv);
	}

	while(1){}	// Infinite loop to not to let any proccess early quit without receiving all signals
}
