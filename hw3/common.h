#ifndef __JOB_H__
#define __JOB_H__

struct Job
{
	int job_flags; // To determine if it is job processing, 0 or queue processing, 1
	int job_type; // 0: encryption; 1: decryption; 2: concatenation
	char *input_file;
	char *output_file;
	char *key;
	int keylen;
	//Config processing parameters	
	int priority;	
};

struct JobDesc
{
	int job_id;
	int job_type;
	int priority;
};

struct JobQInfo
{
	int job_cnt;
	struct JobDesc ** jobs_arr;
};

struct JobInfo
{
	int job_flags; // To determine if it is job processing or queue processing
	int flags; // 0: remove all jobs, 1: remove a single job, 2: list queued jobs, 3: change priority of a job, 4: get number of jobs in queue
	int job_id; // set when flags is 1 or 3
	int priority; // set when flags is 3
	struct JobQInfo *jobq;
};

#endif
