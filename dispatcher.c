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

int setSignalHandler();
void mySignalHandler(int signum, siginfo_t* info, void* ptr);
void killAllProcesses();

ssize_t getFileSize(char* file);
int createCounters(char *charToCount, char *fileName);
int createCounter(char *charToCount, char* fileName, int i, off_t blockSize, off_t blockOffset);
int determinateNumberOfCounters(ssize_t fileSize);
long waitForCounters(int numberOfProcesss);

long readFromPID(pid_t pid);
int openPipe(char* pipeFileName);
long readFromPipe(int pipe);


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

	int numberOfProcesss = createCounters(argv[1], argv[2]);
	if(numberOfProcesss == -1)
		return -1;
    
    charCounter = waitForCounters(numberOfProcesss); //waits for all counters, read the data when they are ready
    if(charCounter == -1) 
    	return -1;
	
	printf("The number of %c in %s is %ld\n", *argv[1], argv[2], charCounter);
		
	return 0;
}

int createCounters(char *charToCount, char *fileName)
{
	ssize_t fileSize = getFileSize(fileName);
	if(fileSize == -1)
		return -1;
	
	int numberOfProcesss = determinateNumberOfCounters(fileSize);

	off_t blockSize = fileSize/numberOfProcesss;
	off_t offset = 0;
	for(int i=0; i<numberOfProcesss-1; i++)
	{
		if(createCounter(charToCount, fileName, i, blockSize, offset) == -1)
			return -1;
		offset+=blockSize;
	}
	if(createCounter(charToCount, fileName, numberOfProcesss-1, fileSize - offset, offset) == -1) //the rest
		return -1;

	return numberOfProcesss;
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

long waitForCounters(int numberOfProcesss)
{
	long charCounter = 0;
	int currentPIDRead = 0, status, wpid;
	do //wait for all process to send signal, once signal recieved it treat it immediately 
	{
	    while(currentPIDRead < currentPIDWrite) //if there are processes that are readly to read
		{
			long temp = readFromPID(readySubprocessArray[currentPIDRead]); //read it fifo
			if(temp == -1) //if failed
			{
				killAllProcesses(); //kill them all
				return -1;
			}
			charCounter += temp;
			currentPIDRead++;
		}
		wpid = wait(&status);
	}while (wpid != -1 || ( wpid == -1 && errno != ECHILD)); //while there are subprocesses running

	
	if(currentPIDRead != numberOfProcesss) //check that all processes read
	{
		printf("Failed to read data from all processes\n");
		return -1;
	}
	return charCounter;
}

long readFromPID(pid_t pid)
{
	char pipeFileName[40];

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
	int pipe;
	do
	{	
		pipe = open(pipeFileName, O_RDONLY);
		if(pipe == -1 && errno != EINTR)
		{
			printf("unable to open a pipe1 - %s\n", strerror(errno));
			return -1;
		}
	}while(pipe == -1);
	return pipe;
}

long readFromPipe(int pipe)
{
	long result;
	void *location = (void *) &result;
	size_t bufSize = sizeof(long);

	while(bufSize>0)
	{
		ssize_t temp = read(pipe, location, bufSize);
		if(temp<0 && errno != EINTR)
		{
			printf("unable to read from pipe - %s\n", strerror(errno));
			return -1;
		}
		location += temp;
		bufSize -= temp;
	}
	return result;
}

int determinateNumberOfCounters(ssize_t fileSize)
{
	if(fileSize <= getpagesize()*2)
		return 1;
	else if(fileSize < 128*1024) //100K
		return 2;
	else if(fileSize < 1024*512) //512K
		return 3;
	else if(fileSize < 1024*1024) //1M
		return 4;
	int numberOfCounters = 3 + fileSize/(1024*1024); 
	
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