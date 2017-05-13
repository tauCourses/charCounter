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
#include <math.h>

#define MAX_NUMBER_OF_PROCESSES 16
#define NUMBER_OR_ARGUMENTS 3
#define basePipeFileName "/tmp/%d"


void mySignalHandler(int signum, siginfo_t* info, void* ptr);
int createCounter(char *charToCount, char* fileName, int i, off_t blockSize, off_t blockOffset);
int setSignalHandler();
long readFromPID(int pid);
int openPipe(char* pipeFileName);
long readFromPipe(int pipe);
ssize_t getFileSize(char* file);
int determinateNumberOfCounters(ssize_t fileSize);
void killAllProcesses();

volatile int currentPIDWrite = 0;
volatile pid_t readySubprocessArray[MAX_NUMBER_OF_PROCESSES]; 

pid_t subprocessArray[MAX_NUMBER_OF_PROCESSES];

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
	if(fileSize == -1)
		return -1;
	
	printf("file size %zu\n", fileSize);

	int numberOfProcesss = determinateNumberOfCounters(fileSize);
	printf("numberOfProcesss - %d\n", numberOfProcesss);
	off_t blockSize = fileSize/numberOfProcesss;
	off_t offset = 0;
	for(int i=0; i<numberOfProcesss-1; i++)
	{
		printf("process %d offset %lu size %lu\n", i, offset, blockSize);
		if(createCounter(argv[1], argv[2], i, blockSize, offset) == -1)
			return -1;
		offset+=blockSize;
	}
	printf("last process offset %lu size %lu\n", offset, fileSize - offset);
	if(createCounter(argv[1], argv[2], numberOfProcesss-1, fileSize - offset, offset) == -1) //the rest
		return -1;

	// printf("ECHILD - %d", ECHILD);
	// printf("EINTR - %d", EINTR);
	// printf("EINVAL - %d", EINVAL);
	// //printf("ECHILD - %d", ECHILD);
	int currentPIDRead = 0, wstatus, wpid;
	do
	{
	    while(currentPIDRead < currentPIDWrite)
		{
			long temp = readFromPID(readySubprocessArray[currentPIDRead]);
			if(temp == -1)
			{
				killAllProcesses();
				return -1;
			}
			charCounter += temp;
			currentPIDRead++;
		}
		wpid = wait(&wstatus);
	//	printf("%d %d\n", wpid, errno);
	}while (wpid != -1 || ( wpid == -1 && errno != ECHILD));

	printf("end %ld\n", charCounter);
	for(int i=0;i<MAX_NUMBER_OF_PROCESSES && subprocessArray[i]!=0;i++)
		printf("%d\t%d\n", subprocessArray[i], readySubprocessArray[i]);
	
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
	if(fileSize<1024)
		return 1;
	int numberOfCounters = (int)fileSize/(1024*4);//(int)(sqrt(fileSize)+0.5);
	printf("sqrt %d\n", numberOfCounters);
	if(numberOfCounters > MAX_NUMBER_OF_PROCESSES)
		numberOfCounters = MAX_NUMBER_OF_PROCESSES;
	return numberOfCounters;
}

ssize_t getFileSize(char* file)
{
	struct stat fileStat;
	if(stat(file, &fileStat) == -1)
	{
		printf("Failed to open file - %s\n", strerror(errno));
		return -1;
	}
	return fileStat.st_size;
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

int createCounter(char *charToCount, char* fileName, int i, off_t blockSize, off_t blockOffset)
{
	pid_t pid = fork();
	if(pid<0)
	{
		printf("Fork failed - %s\n", strerror(errno));
		killAllProcesses();
		return -1;
	}
	else if(pid == 0)
	{
		char istr[10]; 
		sprintf(istr, "%d", i);

		char sizeStr[20]; 
		sprintf(sizeStr, "%ld", blockSize);

		char offsetStr[20]; 
		sprintf(offsetStr, "%ld", blockOffset);

		char *argv[] = {"counter", charToCount, fileName, offsetStr , sizeStr, istr, NULL};
		if(execv(argv[0], argv) == -1)
			printf("execute counter failed on procees number %d - %s\n", i, strerror(errno));
		killAllProcesses();
		return -1;
	}
	subprocessArray[i] = pid;
}

void mySignalHandler(int signum, siginfo_t* info, void* ptr)
{
	readySubprocessArray[currentPIDWrite] = info->si_pid;
	currentPIDWrite++;
}

void killAllProcesses()
{
	printf("kill all sub processes!\n");
	for(int i=0;i<MAX_NUMBER_OF_PROCESSES;i++)
	{
		if(subprocessArray[i] == 0)
			break;
		kill(subprocessArray[i],SIGKILL);
	}
}