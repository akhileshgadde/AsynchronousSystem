#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "job.h"

#define MAX_NAME_LEN 256

/* Generates the Job structure to pass through the kernel
 * Returns NULL in case of any error
 */
struct Job* createJob(int type, const char* infile, const char* outfile, const char *key, int priority)
{
	struct Job *job = NULL;

	job = (struct Job*) malloc(sizeof(struct Job));
	if(!job)
		return NULL;

	job->job_flags = 0;	
	job->job_type = type;	
	
	job->input_file = (char*) malloc(strlen(infile)+1);
	strcpy(job->input_file, infile);
	job->input_file[strlen(job->input_file)] = '\0';
	
	job->output_file = (char*) malloc(strlen(outfile)+1);
	strcpy(job->output_file, outfile);
	job->output_file[strlen(job->output_file)] = '\0';
	
	job->key = (char*) malloc(strlen(key)+1);
    strcpy(job->key, key);
    job->key[strlen(job->key)] = '\0';

	job->keylen = strlen(key);
	job->priority = priority;

	return job;  
}

/* Generates the JobInfo structure to pass through the kernel
 * Returns NULL in case of any error
 */
struct JobInfo* createJobInfo(int flags, int job_id, int priority)
{
	struct JobInfo *jobinfo = NULL;

	jobinfo = (struct JobInfo*) malloc(sizeof(struct JobInfo));
	if(!jobinfo)
		return NULL;

	jobinfo->job_flags = 1;
	jobinfo->flags = flags;
	jobinfo->job_id = job_id;
	jobinfo->priority = priority;

	return jobinfo;
}

/* Gets char buffers from the user */
static void getInputBuffers(char inputfile[], char outputfile[], char key[])
{
	printf("Enter input file:\n");
	scanf("%s", inputfile); /***change scanf to fgets***/
	inputfile[strlen(inputfile)] = '\0';
	
	printf("Enter output file:\n");
	scanf("%s", outputfile);
	outputfile[strlen(outputfile)] = '\0';
	
	printf("Enter a key:\n");
	scanf("%s", key);
	key[strlen(key)] = '\0';
}

void* processEnDecryptReq(int flags)
{
	/** change to PATHMAX **/
	char inputfile[MAX_NAME_LEN], outputfile[MAX_NAME_LEN], key[MAX_NAME_LEN];
	struct Job *job = NULL;
	int priority;

	getInputBuffers(inputfile, outputfile, key);

	//printf("Input file: %s Output file: %s Key: %s\n", inputfile, outputfile, key);
	//printf("Len: Input file: %d Output file: %d Key: %d\n", strlen(inputfile), strlen(outputfile), strlen(key));

	printf("Enter job priority (Integer greater than 0)\n");
	scanf("%d", &priority);

	job = createJob(flags, inputfile, outputfile, key, priority);
	
	return (void*) job;
}

void* processRemoveReq(void)
{
	struct JobInfo *jobinfo = NULL;
	char choice;
	int jobid;

	printf("Do you want to remove all jobs from the queue [Y/y/N/n]\n");
	scanf("%c", &choice);
	getchar();

	if(choice == 'y' || choice == 'Y')
		jobinfo = createJobInfo(0, -1, -1);
		
	else if(choice == 'n' || choice == 'N')
	{
		printf("Do you want to check all the jobs in the queue [Y/y/N/n]\n");
		scanf("%c", &choice);
		getchar();
		
		if(choice == 'y' || choice == 'Y')
		{
			jobinfo = createJobInfo(2, -1, -1); /** Need to change **/

			printf("Enter the job id to remove:\n");
			scanf("%d", &jobid);
			
			jobinfo = createJobInfo(1, jobid, -1);
		}
		
		else if(choice == 'n' || choice == 'N')
		{
			printf("Enter job id to remove:\n");
			scanf("%d", &jobid);

			jobinfo = createJobInfo(1, jobid, -1);
		}
	
		else
			printf("Invalid choice.\n");
	}
	else
		printf("Invalid choice.\n");

	return (void*) jobinfo;
}

void* processListReq(void)
{
	struct JobInfo *jobinfo = NULL;

	jobinfo = createJobInfo(2, -1, -1);
	
	return (void*) jobinfo;
}

void* processChangePriorityReq(void)
{
	struct JobInfo *jobinfo = NULL;
	char choice;
	int jobid, priority;

	printf("Do you want to check all the jobs in the queue [Y/y/N/n]\n");
	scanf("%c", &choice);
	getchar();

	if(choice == 'y' || choice == 'Y')
	{
		jobinfo = createJobInfo(2, -1, -1); /** Need to change **/

		printf("Enter the job id to change priority:\n");
		scanf("%d", &jobid);

		printf("Enter the priority:\n");
		scanf("%d", &priority);
		
		jobinfo = createJobInfo(3, jobid, priority);
	}

	else if(choice == 'n' || choice == 'N')
	{
		printf("Enter job id to change priority\n");
		scanf("%d", &jobid);

		printf("Enter the priority:\n");
        scanf("%d", &priority);

		jobinfo = createJobInfo(3, jobid, priority);
	}

	else
		printf("Invalid choice.\n");	
	
	return (void*) jobinfo;	
}

/** Need to handle bad inputs from the user: like priority <= 0 etc. **/
void* getUserInput(int *err)
{
	char choice;
	void* ret = NULL;
	
	printf("=============================\n");
	printf("Enter your choice of job:\n");
	printf("[E/e]ncryption\n");
	printf("[D/d]ecrytion\n");
	printf("[R/r]emove a job\n");
	printf("[L/l]ist jobs.\n");
	printf("[C/c]hange priority of a job.\n");
	printf("[Q/q]uit\n");
	printf("=============================\n");
		
	scanf("%c", &choice);
	getchar();
	
	if(choice == 'e' || choice == 'E')
		ret = processEnDecryptReq(0);

	else if(choice == 'd' || choice == 'D')
		ret = processEnDecryptReq(1);

	else if(choice == 'r' || choice == 'R') /*** need to handle the case when multiple job ids can be removed ***/ 
		ret = processRemoveReq();

	else if(choice == 'l' || choice == 'L')
		ret = processListReq();

	else if(choice == 'c' || choice == 'C')
		ret = processChangePriorityReq();

	else if(choice == 'q' || choice == 'Q')
		goto out;		

	else
		printf("Invalid choice.\n");
		
	/*** Need to free memory for in out files and key?? ***/
	if(!ret)
		*err = -1;
out:
	return ret;
}
