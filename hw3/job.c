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
struct JobInfo* createJobInfo(int flags, int job_id, int priority, struct JobQInfo *jobqinfo)
{
	struct JobInfo *jobinfo = NULL;

	jobinfo = (struct JobInfo*) malloc(sizeof(struct JobInfo));
	if(!jobinfo)
		return NULL;

	jobinfo->job_flags = 1;
	jobinfo->flags = flags;
	jobinfo->job_id = job_id;
	jobinfo->priority = priority;
	jobinfo->jobq = jobqinfo;

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

	printf("Enter job priority (Integer between 0 and 256)\n");
	scanf("%d", &priority);
	
	if(priority <=0 || priority > 256)
	{
		printf("Invalid priority\n");
		return NULL;
	}

	job = createJob(flags, inputfile, outputfile, key, priority);
	
	return (void*) job;
}

void* processGetNumJobs(void)
{
	struct JobInfo *jobinfo = NULL;
	
	jobinfo = createJobInfo(4, -1, -1, NULL);

    return (void*) jobinfo;
}

void* processListReq(int jobcnt)
{
	int i;
	struct JobInfo *jobinfo = NULL;
	struct JobQInfo *jobqinfo = NULL;

	jobqinfo = (struct JobQInfo*) malloc(sizeof(struct JobQInfo));
	
	jobqinfo->job_cnt = jobcnt;
	jobqinfo->jobs_arr = (struct JobDesc**) malloc(sizeof(struct JobDesc*) * jobcnt);
	
	for(i=0; i<jobcnt; i++)
	{
		jobqinfo->jobs_arr[i] = (struct JobDesc*) malloc(sizeof(struct JobDesc));
		memset(jobqinfo->jobs_arr[i], 0, sizeof(struct JobDesc));
	}
			
	jobinfo = createJobInfo(2, -1, -1, jobqinfo);
	
	return (void*) jobinfo;
}
