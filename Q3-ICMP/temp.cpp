#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

using namespace std;

void sig_handler(int signo)
{
    printf("SIGALRM Received\n");
}

void* temp(void* d)
{
    sigset_t *set = (sigset_t*)d;
    int s, sig;
    alarm(1);

    while (true)
    {
        s = sigwait(set, &sig);
	if (s != 0)
		perror("sigwait");

	printf("Signal number received in thread: %d\n", sig);
    }    
    
    return nullptr;
}

int main()
{
    sigset_t st;
    
    sigemptyset(&st);
    sigaddset(&st, SIGALRM);
    sigaddset(&st, SIGUSR1);
    sigaddset(&st, SIGQUIT);
    int s = pthread_sigmask(SIG_BLOCK, &st, NULL);

    if (s != 0)
	    perror("sigmask error");

    struct sigaction new_action { .sa_flags = 0 };	// flags set to 0 so that syscalls can be interrupted, by default they are not interrupted by signals if handler is attached
    new_action.sa_handler = sig_handler;
    sigaction(SIGALRM, &new_action, NULL);

    pthread_t th;
    pthread_create(&th, nullptr, temp, &st);
    pthread_join(th, nullptr);
    
    return 0;
}
