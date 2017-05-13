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

#define NUMBER_OF_ARGUMENTS 6
#define PIPE_FILE_NAME "/tmp/%d"
#define BUFFER_SIZE 1024

int parseOff_t(char* str, off_t* arg);
int openFile(char* fileName);
long countChar(int file, char c, off_t offset, off_t size);
int sendSignal(int sleepTime);
int openPipe(char* pipeFileName);
int writeToPipe(int pipe,long charCounter);	
int closePipe(int pipe, char* pipeFileName);

int main(int argc, char** argv)
{
	off_t blockOffset, blockSize;
	int file, pipe;
	char pipeFileName[80];

	if(argc != NUMBER_OF_ARGUMENTS)
	{
		printf("Wrong number of arguments\n");
		return -1;
	}

	if(parseOff_t(argv[3], &blockOffset) == -1)
		return -1;
	if(parseOff_t(argv[4], &blockSize) == -1)
		return -1;
	
	file = openFile(argv[2]);
	if(file == -1)
		return -1;

	long charCounter = countChar(file, *argv[1], blockOffset, blockSize);
	if(charCounter == -1)
		return -1;

	if(close(file) == -1)
	{
		printf("Failed to close file - %s\n", strerror(errno));
		return -1;
	}

	sprintf(pipeFileName, PIPE_FILE_NAME, getpid()); 
	
	if(mkfifo(pipeFileName, 0666) == -1) //create a fifo pipe
	{
		printf("unable to create a pipe - %s\n", strerror(errno));
		return -1;
	}

	if(sendSignal(atoi(argv[5])) == -1)
		return -1;

	pipe = openPipe(pipeFileName);
	if(pipe == -1)
		return -1;

	if(writeToPipe(pipe, charCounter) == -1)
		return -1;

 	
	if(closePipe(pipe, pipeFileName) == -1)
		return -1;

 	return 0;
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

int sendSignal(int sleepTime)
{
	usleep(1000*sleepTime);
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
	char buf[20];
	sprintf(buf, "%ld", charCounter);
	size_t bufSize = strlen(buf);
	void *location = (void *)buf;
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
	usleep(10*1000); //10ms before closing the pipe and delete the file
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
	if(sscanf(str, "%zu", arg) != 1)
	{
		printf("unable to parse %s\n",str);
		return -1;
	}
	return 0;
	
}