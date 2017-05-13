#include <stdio.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>


#define MAX_NUMBER_OF_PROCESSES 16
#define NUMBER_OR_ARGUMENTS 3
#define basePipeFileName "/tmp/%d"


void mySignalHandler(int signum, siginfo_t* info, void* ptr);
int createCounter(char *charToCount, char* fileName, int i);
int setSignalHandler();
long readFromPID(int pid);
int openPipe(char* pipeFileName);
long readFromPipe(int pipe);
ssize_t getFileSize(char* file);
int determinateNumberOfCounters(ssize_t fileSize);

volatile int currentPIDWrite = 0;
volatile int pidArray[MAX_NUMBER_OF_PROCESSES]; 

int main(int argc, char** argv)
{
	long charCounter = 0;
	if(argc != NUMBER_OR_ARGUMENTS)
	{
		printf("Wrong number of arguments\n");
		return -1;
	}

	if(setSignalHandler() == -1)
		return -1;

	ssize_t fileSize = getFileSize(argv[2]);

	int numberOfProcesss = determinateNumberOfCounters(fileSize);

	for(int i=0; i<numberOfProcesss; i++)
		if(createCounter(argv[1], argv[2], i) == -1)
			return -1;

	int currentPIDRead = 0, wstatus;
	for(int i=0; i<numberOfProcesss; i++)
	{
		do
		{
			wait(&wstatus) != -1;
			if(currentPIDRead < currentPIDWrite)
			{
				charCounter += readFromPID(pidArray[currentPIDRead]);
				currentPIDRead++;
			}
		}while(!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus));
	}
	
	printf("end %ld\n", charCounter);
	return 0;
}

long readFromPID(int pid)
{
	char pipeFileName[80];

	sprintf(pipeFileName, basePipeFileName, pid); 
	
	int pipe = openPipe(pipeFileName);
	if(pipe == -1)
		return -1;

	long result = readFromPipe(pipe);
	
	if(close(pipe) == -1)
	{
		printf("Failed to close pipe - %s\n", strerror(errno));
		return -1;
	}

	return result;
}

int openPipe(char* pipeFileName)
{
	int pipe = open(pipeFileName, O_RDONLY);
	if(pipe == -1)
	{
		printf("unable to open a pipe1 - %s\n", strerror(errno));
		return -1;
	}
	return pipe;
}

long readFromPipe(int pipe)
{
	char buf[20];
	void *location = buf;
	ssize_t len = read(pipe, location, 20);	
	long result;
	if(len == -1)
	{
		printf("Error reading from pipe: %s\n", strerror(errno));
		return -1;	
	}
	if(sscanf(buf, "%ld", &result) != 1)
	{
		printf("unable to parse data from pipe %s\n",buf);
		return -1;
	}
	return result;
}
int determinateNumberOfCounters(ssize_t fileSize)
{
	int numberOfCounters = fileSize/7;

	if(numberOfCounters > MAX_NUMBER_OF_PROCESSES)
		numberOfCounters = MAX_NUMBER_OF_PROCESSES;
	return 4;
}

ssize_t getFileSize(char* file)
{
	return 1024;
}

int setSignalHandler()
{
	struct sigaction new_action;
	memset(&new_action, 0, sizeof(new_action));
	new_action.sa_sigaction = mySignalHandler;
	new_action.sa_flags = SA_SIGINFO;
	if(0 != sigaction(SIGUSR1, &new_action, NULL))
	{
		printf("Signal handle registration failed - %s\n",strerror(errno));
		return -1;
	}
	return 0;
}

int createCounter(char *charToCount, char* fileName, int i)
{
	pid_t pid = fork();
	if(pid<0)
	{
		printf("Fork failed - %s\n", strerror(errno));
		return -1;
	}
	else if(pid == 0)
	{
		char str[10]; 
		sprintf(str, "%d", i);

		char *argv[] = {"counter", charToCount, fileName, "10" , "5", str, NULL};
		if(execv(argv[0], argv) == -1)
			printf("execute counter failed on procees number %d - %s\n", i, strerror(errno));
		
		return -1;
	}
}

void mySignalHandler(int signum, siginfo_t* info, void* ptr)
{
	pidArray[currentPIDWrite] = info->si_pid;
	currentPIDWrite++;
}