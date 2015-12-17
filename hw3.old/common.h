#ifndef __JOB_H__
#define __JOB_H__

struct Job
{
	int job_flags; // To determine if it is job processing, 0 or queue processing, 1
	int job_type; // 0: encryption; 1: decryption
	char *input_file;
	char *output_file;
	char *key;
	int keylen;
	//Config processing parameters	
	int priority;	
};

struct JobInfo
{
	int job_flags; // To determine if it is job processing or queue processing
	int flags; // 0: remove all jobs, 1: remove a single job, 2: list queued jobs, 3: change priority of a job
	int job_id; // set when flags is 1 or 3
	int priority; // set when flags is 3
};

struct JobReturn
{
	int job_type;
	char input_file[4096];
	//char output_file[4096];
	int ret;
};

struct JobReturn job_ret;
#define __func__ __FUNCTION__
#endif
