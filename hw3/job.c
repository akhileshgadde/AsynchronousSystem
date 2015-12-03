#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/md5.h>
#include <errno.h>
#include "job.h"

#define MAX_NAME_LEN 256

/* Generates the Job structure to pass through the kernel
 * Returns NULL in case of any error
 */
struct Job* createJob(int type, const char* infile, const char* outfile, const char *key, int priority)
{
	struct Job *job = NULL;

	MD5_CTX mc;
    unsigned char hash_key[MD5_DIGEST_LENGTH];
	
	job = (struct Job*) malloc(sizeof(struct Job));
	if(!job)
		return NULL;
	
	memset(job, 0, sizeof(struct Job));
	
	job->job_flags = 0;	
	job->job_type = type;	
	
	job->input_file = (char*) malloc(strlen(infile)+1);
	if(!job->input_file)
		return NULL;
	
	memset(job->input_file, 0, sizeof(job->input_file));
	
	strcpy(job->input_file, infile);
	job->input_file[strlen(job->input_file)] = '\0';
	
	job->output_file = (char*) malloc(strlen(outfile)+1);
	if(!job->output_file)
		return NULL;
	
	memset(job->output_file, 0, sizeof(job->output_file));
	
	strcpy(job->output_file, outfile);
	job->output_file[strlen(job->output_file)] = '\0';

	MD5_Init(&mc);
    MD5_Update(&mc, key, strlen(key));
    MD5_Final(hash_key, &mc);
	
	job->key = (char*) malloc(MD5_DIGEST_LENGTH+1);
	if(!job->key)
	{
		printf("Error while allocating memory for the key\n");
        return NULL;
	}
	
	memset(job->key, 0, sizeof(job->key));
    memcpy(job->key, hash_key, sizeof(job->key));
	job->key[16] = '\0';
    
	job->keylen = MD5_DIGEST_LENGTH;
	
	job->priority = priority;

	#if 0
	printf("Job details are:\n");
	printf("Input file: %s\n", job->input_file);
	printf("Output file: %s\n", job->output_file);
	printf("Key: %s\n", job->key);
	#endif

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
	
	do{
		errno = 0;
		scanf("%s", inputfile);
	}while(errno == EINTR);
 
	inputfile[strlen(inputfile)] = '\0';
	
	printf("Enter output file:\n");
	do{
		errno = 0;
		scanf("%s", outputfile);
	}while(errno == EINTR);
	
	outputfile[strlen(outputfile)] = '\0';
	
	printf("Enter a key:\n");
	do{
		errno = 0;
		scanf("%s", key);
	}while(errno == EINTR);
	
	key[strlen(key)] = '\0';
}

void* processEnDecryptReq(int flags)
{
	char inputfile[MAX_NAME_LEN], outputfile[MAX_NAME_LEN], key[MAX_NAME_LEN];
	struct Job *job = NULL;
	int priority;

	getInputBuffers(inputfile, outputfile, key);

	printf("Enter job priority (Integer between 0 and 256)\n");
	do{
		errno = 0;
		scanf("%d", &priority);
	}while(errno == EINTR);
	
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
	memset(jobqinfo, 0, sizeof(struct JobQInfo));
 	
	jobqinfo->job_cnt = jobcnt;
	jobqinfo->jobs_arr = (struct JobDesc**) malloc(sizeof(struct JobDesc*) * jobcnt);
	memset(jobqinfo->jobs_arr, 0, sizeof(struct JobDesc*) * jobcnt);
	
	for(i=0; i<jobcnt; i++)
	{
		jobqinfo->jobs_arr[i] = (struct JobDesc*) malloc(sizeof(struct JobDesc));
		memset(jobqinfo->jobs_arr[i], 0, sizeof(struct JobDesc));
	}
			
	jobinfo = createJobInfo(2, -1, -1, jobqinfo);
	
	return (void*) jobinfo;
}
