#include <stdio.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>

#define MIN_NUMBER_OF_ARGUMENTS 5
#define PIPE_FILE_NAME "/tmp/%d"
#define BUFFER_SIZE 1024

//functions to get the data:
long getCharAmountFromFile(char* charToCount, char* fileName, char* offsetStr, char* sizeStr);
int parseOff_t(char* str, off_t* arg);
int openFile(char* fileName);
long countChar(int file, char c, off_t offset, off_t size);

//functions for sending the data back:
int sendAmountToDispatcher(int waitTime, long charCounter); 
int sendSignal(int sleepTime);
int openPipe(char* pipeFileName);
int writeToPipe(int pipe,long charCounter);	
int closePipe(int pipe, char* pipeFileName);


int main(int argc, char** argv)
{
	if(argc < MIN_NUMBER_OF_ARGUMENTS)
	{
		printf("Wrong number of arguments\n");
		return -1;
	}	

	long charCounter = getCharAmountFromFile(argv[1], argv[2], argv[3], argv[4]);
	if(charCounter == -1)
		return -1;

	int waitTime = (argc==6)?atoi(argv[5]):getpid()%100;
	if(sendAmountToDispatcher(waitTime, charCounter) == -1)
		return -1;

 	return 0;
}

long getCharAmountFromFile(char* charToCount, char* fileName, char* offsetStr, char* sizeStr)
{
	long charCounter;
	off_t blockOffset, blockSize;

	if(parseOff_t(offsetStr, &blockOffset) == -1)
		return -1;
	if(parseOff_t(sizeStr, &blockSize) == -1)
		return -1;

	int file = openFile(fileName);
	if(file == -1)
		return -1;

	charCounter = countChar(file, *charToCount, blockOffset, blockSize);
	if(charCounter == -1)
		return -1;

	if(close(file) == -1)
	{
		printf("Failed to close file - %s\n", strerror(errno));
		return -1;
	}

	return charCounter;
}

int openFile(char* fileName)
{
	int file = open(fileName, O_RDONLY);
	if(file == -1)
	{
		printf("unable to open a pipe2 - %s\n", strerror(errno));
		return -1;
	}
	return file;
}

long countChar(int file, char c, off_t offset, off_t size)
{
	long counter = 0;
	char buf[BUFFER_SIZE];
	if(lseek(file, offset, SEEK_SET) < 0) 
	{
		printf("Error seek in file: %s\n", strerror(errno));
		return -1;
	}

	char* writeBufferPointer = buf;
	while(size > 0) //while more bytes need to be processed
	{
		ssize_t len;
		if(size<BUFFER_SIZE) //read the minimum between readingSize and bufferSize
			len = read(file, buf, size);
		else
			len = read(file, buf, BUFFER_SIZE);

		if(len < 0) //check that the read call succeeded 
		{
			printf("Error reading from file: %s\n", strerror(errno));
			return -1;
		}
		size -= len;
		for(int i=0; i<len; i++) 
			if(buf[i] == c) //check char 
				counter++;
	}
	
	return counter;
}

int sendAmountToDispatcher(int waitTime, long charCounter)
{
	char pipeFileName[80];
	sprintf(pipeFileName, PIPE_FILE_NAME, getpid()); 
	
	if(mkfifo(pipeFileName, 0666) == -1) //create a fifo pipe
	{
		printf("unable to create a pipe - %s\n", strerror(errno));
		return -1;
	}

	if(sendSignal(waitTime) == -1)
		return -1;

	int pipe = openPipe(pipeFileName);
	if(pipe == -1)
		return -1;

	if(writeToPipe(pipe, charCounter) == -1)
		return -1;

 	
	if(closePipe(pipe, pipeFileName) == -1)
		return -1;

	return 0;
}
int sendSignal(int sleepTime)
{
	usleep(1000*sleepTime*50); //each process wait a different time before sending the signal. 
	if(kill(getppid(), SIGUSR1) == -1)
	{
		printf("Failed to send signal - %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

int openPipe(char* pipeFileName)
{
	
	int pipe = open(pipeFileName, O_WRONLY);
	if(pipe == -1)
	{
		printf("unable to open a pipe2 - %s\n", strerror(errno));
		return -1;
	}
	return pipe;
}

int writeToPipe(int pipe,long charCounter)
{
	size_t bufSize = sizeof(long);
	void *location = (void *)(&charCounter);
	do
	{
		ssize_t temp = write(pipe,location,bufSize);
		if(temp <= 0)
		{
			printf("Error Writing to pipe: %s\n", strerror(errno));
			return -1;	
		}
		bufSize -= temp;
		location += temp;
	} while(bufSize>0);

	return 0;
}

int closePipe(int pipe, char* pipeFileName)
{
	usleep(1000*50); //50ms before closing the pipe and delete the file
	if(close(pipe) == -1)
	{
		printf("Failed to close pipe - %s\n", strerror(errno));
		return -1;
	}

	if(unlink(pipeFileName) == -1)
	{
		printf("Failed to remove pipe file - %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

int parseOff_t(char* str, off_t* arg)
{
	if(sscanf(str, "%lu", arg) != 1)
	{
		printf("unable to parse %s\n",str);
		return -1;
	}
	return 0;
	
}