#ifndef __SYS_JOB_H__
#define __SYS_JOB_H__

#include "common.h" 
int sys_submitjob(void*, int);

struct kJob* copy_job_from_user(void *args);
struct JobInfo* copy_jobinfo_from_user(void *args);
int producer(void*);
int consumer(void*);
int processJobQueueRequest(struct JobInfo*);

struct kJob
{
	int job_id;
	struct Job *job;
};

struct job_queue
{
	struct list_head list;
	void *work;
};

struct job_queue* getHighestPriorityJob(void);

/* Job queue processing functions */
int getJobsFromQueue(struct JobInfo*);
struct job_queue* findJobInQueue(int);
int removeAllJobs(void);
int removeSingleJob(int);
int changeJobPriorityInQueue(int, int);
int processJobQueueRequest(struct JobInfo*);

#endif
