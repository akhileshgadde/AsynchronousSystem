#include <asm/unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>
#include "job.h"

#ifndef __NR_submit_job
#error submit_job system call not defined
#endif

int main(int argc, const char *argv[])
{
	int ret = 0;
	void *data;
	
	data = getUserInput(&ret);
	if(ret < 0)
	{
		printf("Error processing the user input.\n");
		exit(EXIT_FAILURE);
	} 
	ret = syscall(__NR_submit_job, data);
	if (ret == 0)
		printf("syscall returned %d\n", ret);
	else
		printf("syscall returned %d (errno=%d)\n", ret, errno);
		
	exit(0);
}

