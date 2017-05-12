#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

void my_signal_handler(int signum, siginfo_t* info, void* ptr)
{
	printf("signal from %lu\n", (unsigned long) info->si_pid);
}

int main(int argc, char** argv)
{
	struct sigaction new_action;
	memset(&new_action, 0, sizeof(new_action));
	new_action.sa_sigaction = my_signal_handler;
	new_action.sa_flags = SA_SIGINFO;
	if(0 != sigaction(SIGUSR1, &new_action, NULL))
	{
		printf("Signal handle registration failed - %s\n",strerror(errno));
		return -1;
	}

	for(int i=0; i<3; i++)
	{
		pid_t pid = fork();
		if(pid<0)
		{
			printf("Fork failed - %s\n", strerror(errno));
			return -1;
		}
		else if(pid == 0)
		{
			printf("son child number %d pid %lu\n", i, (unsigned long) pid);
			char *argv[] = {"counter", "a", "aaa.txt", "10" , "5"};
			if(execv(argv[0], argv) == -1)
				printf("execute counter failed on procees number %d - %s\n", i, strerror(errno));
			
			return -1;
		}
		else
			printf("Created a new process!\n");

	}
	return 0;
}