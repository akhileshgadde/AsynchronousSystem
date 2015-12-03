#include <asm/unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "usr_netlink.h"
#include "job.h"

#ifndef __NR_submit_job
#error submit_job system call not defined
#endif

int listJobs(void)
{
	void *data = NULL;
	int i = 0, jobct, ret = 0;
	struct JobDesc *temp;
	
	data = processGetNumJobs();
	if(data == NULL)
	{
		ret = -1;
		goto out;	
	}
	
	jobct = syscall(__NR_submit_job, data);
	printf("Size of list returned from the kernel: %d\n", jobct);
	
	if(jobct < 0)
	{
		ret = jobct;
		goto out;
	}
	
	if(jobct == 0)
	{
		printf("There are no jobs in the queue.\n");
		ret = -1;
		goto out;
	}
	
	data = processListReq(jobct);
	if(data == NULL)
    {
        ret = -1;
        goto out;
    }

	printf("========Jobs in the queue======\n");
	ret = syscall(__NR_submit_job, data);
	if(ret < 0)
		goto out;

	jobct = ((struct JobInfo*) data)->jobq->job_cnt;

	while(i < jobct)
	{
		temp = ((struct JobInfo*) data)->jobq->jobs_arr[i];
		if(temp != NULL)
		{
			printf("Job id: %d\n", temp->job_id);
			if(temp->job_type == 0)
				printf("Job type: Encryption\n");
			else if(temp->job_type == 1)
				printf("Job type: Decryption\n");
			else
				printf("Job type: Concatenation\n");
			printf("Job priority: %d\n", temp->priority);
			printf("\n");
		}
		
		i++;
	}

out:	
	if(data) // Need to remove the array that is allocated too
	{
		printf("Free data\n");
		if(((struct JobInfo*) data)->jobq)
		{
			i = 0;
			while(i < jobct)
			{
				if(((struct JobInfo*) data)->jobq->jobs_arr[i])
					free(((struct JobInfo*) data)->jobq->jobs_arr[i]);
				i++;
			}
			if(((struct JobInfo*) data)->jobq->jobs_arr)
				free(((struct JobInfo*) data)->jobq->jobs_arr);
			if(((struct JobInfo*) data)->jobq)
				free(((struct JobInfo*) data)->jobq);
		}	
		free(data);
	}
	
	return ret;
}

int removeAllJobs(void)
{
	int ret = 0;
	struct JobInfo *jobinfo = NULL;

	jobinfo = createJobInfo(0, -1, -1, NULL);
	if(!jobinfo)
	{
		ret = -1;
		goto out;
	}
    
	ret = syscall(__NR_submit_job, (void*) jobinfo);

out:
	if(jobinfo)
		free(jobinfo);
	return ret;
}

int removeSingleJob(void)
{
	int ret = 0;
	struct JobInfo *jobinfo = NULL;
	int jobid;

	printf("Enter the job id to remove:\n");
	do{
		errno = 0;
		scanf("%d", &jobid);
	}while(errno == EINTR);
	
	jobinfo = createJobInfo(1, jobid, -1, NULL);
	if(!jobinfo)
	{
		ret = -1;
		goto out;
	}

	ret = syscall(__NR_submit_job, (void*) jobinfo);

out:
	if(jobinfo)
		free(jobinfo);
	return ret;
}

int removeJobs(void)
{
	char choice;
	int ret = 0;

	printf("Do you want to remove all jobs from the queue [Y/y/N/n]\n");
	do{
		errno = 0;
		scanf("%c", &choice);
	}while(errno == EINTR);
	getchar();

	if(choice == 'y' || choice == 'Y')
		ret = removeAllJobs();
		
	else if(choice == 'n' || choice == 'N')
	{
		printf("Do you want to check all the jobs in the queue [Y/y/N/n]\n");
		do{
			errno = 0;
			scanf("%c", &choice);
		}while(errno == EINTR);
		getchar();
		
		if(choice == 'y' || choice == 'Y')
		{
			ret = listJobs();
			if(ret < 0)
				goto out;
			
			ret = removeSingleJob();
		}
		
		else if(choice == 'n' || choice == 'N')
			ret = removeSingleJob();
	
		else
			printf("Invalid choice.\n");
	}
	else
		printf("Invalid choice.\n");

out:
	return ret;
}

int __changeJobPriority(void)
{
	int ret = 0;
    struct JobInfo *jobinfo = NULL;
    int jobid, priority;

	printf("Enter the job id:\n");
	do
	{
		errno = 0;
		scanf("%d", &jobid);
	}while(errno == EINTR);

	printf("Enter the job priority (Integer between 1 and 256)\n");
	do
    {
        errno = 0;
		scanf("%d", &priority);
	}while(errno == EINTR);
	
	if(priority <= 0 || priority > 256)
	{
		printf("Invalid priority\n");
		ret = -EINVAL;
		goto out;
	}
	
	jobinfo = createJobInfo(3, jobid, priority, NULL);
    if(!jobinfo)
    {
        ret = -1;
        goto out;
    }

    ret = syscall(__NR_submit_job, (void*) jobinfo);

out:
    if(jobinfo)
        free(jobinfo);
    return ret;
}

int changeJobPriority(void)
{
	char choice;
    int ret = 0;

	printf("Do you want to check all the jobs in the queue [Y/y/N/n]\n");
	do{
		errno = 0;
		scanf("%c", &choice);
	}while(errno == EINTR);
	getchar();

	if(choice == 'y' || choice == 'Y')
	{
		ret = listJobs();
		if(ret < 0)
			goto out;

		__changeJobPriority();
	}
	
	else if(choice == 'n' || choice == 'N')
        ret = __changeJobPriority();

	else
		printf("Invalid choice.\n");

out:
	return ret;
}

/** Need to handle bad inputs from the user: like priority <= 0 etc. **/
int main(int argc, const char *argv[])
{
	int ret = 0;
	void *data = NULL;
	char choice;
	pthread_t t_netlink;
	int rand_flag = 0;
	int pid = getpid();
	
	if(pthread_create(&t_netlink, NULL, netlink_process, (void *) &pid) != 0) 
	{ 
		printf("Thread creation error.\n"); 
		ret = -errno; 
		goto out; 
	}
	
		
	while(1)
	{
		set_sigusr1_signal();
			
		printf("\n=============================\n");
		printf("Enter your choice of job:\n");
		printf("[E/e]ncryption\n");
		printf("[D/d]ecrytion\n");
		printf("[R/r]emove a job\n");
		printf("[L/l]ist jobs.\n");
		printf("[C/c]hange priority of a job.\n");
		printf("[Q/q]uit\n");
		printf("=============================\n");
		
		do{	
			errno = 0;
			scanf("%c", &choice);
		}while(errno == EINTR);
		
		if(choice == 'e' || choice == 'E')
		{
			data = processEnDecryptReq(0);
			if(!data)
				goto out;	
			
			ret = syscall(__NR_submit_job, data);
		}
		
		else if(choice == 'd' || choice == 'D')
		{
			data = processEnDecryptReq(1);
			if(!data)
				goto out;

			ret = syscall(__NR_submit_job, data);
		}

		else if(choice == 'r' || choice == 'R') /*** need to handle the case when multiple job ids can be removed ***/ 
			ret = removeJobs();

		else if(choice == 'l' || choice == 'L')
			ret = listJobs();		

		else if(choice == 'c' || choice == 'C')
			ret = changeJobPriority();

		else if(choice == 'q' || choice == 'Q')
			break;	
		else
		{
			printf("Invalid choice.\n");
			continue;			
		}
more:
		printf("Do you want to enter more jobs(Y/y/N/n)?\n");
		do
		{
			errno = 0;
			scanf("%c", &choice);
		}while(errno == EINTR);

		if(choice == 'N' || choice == 'n')
		{
			rand_flag = 1;
			break;
		}

		else if(choice != 'Y' || choice != 'y')
			goto more; 	
	}		

	if(rand_flag == 1)
		do_random_work();

out:
	printf("Cleaning up memory\n");
	if(data)
	{
		if(((struct Job*) data)->input_file)
			free(((struct Job*) data)->input_file);
		if(((struct Job*) data)->output_file)
			free(((struct Job*) data)->output_file);
		if(((struct Job*) data)->key)
			free(((struct Job*) data)->key);
		free(data);
	}
	
	if(ret < 0)
	{
		printf("syscall returned %d (errno=%d) \n", ret, errno);
		exit(EXIT_FAILURE);
	}
	else
	{
		printf("syscall returned successfully \n");
		exit(0);
	}
}

void do_random_work()
{
	int incr = 0; 
	
	while (1) 
	{ 
		incr++; 
		if (incr == 32765) 
			incr = 0; 
	} 
}

