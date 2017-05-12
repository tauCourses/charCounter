#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

int main(int argc, char** argv)
{
	printf("counter! yayy\n");
	pid_t parentPid =  getppid();
	printf("parents pid %lu\n", (unsigned long) parentPid);
	kill(parentPid, SIGUSR1);
	return -1;
}