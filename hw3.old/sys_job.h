#ifndef __SYS_JOB_H__
#define __SYS_JOB_H__

#include "common.h" 
int sys_submitjob(void*, int);

struct kJob
{
    int job_id;
    struct Job *job;
};

struct kJob* copy_job_from_user(void *args);
struct JobInfo* copy_jobinfo_from_user(void *args);
int producer(void*);
int consumer(void*);

struct job_queue
{
	struct list_head list;
	void *work;
};

static int pid;
struct job_queue* getHighestPriorityJob(void);
void netlink_send_msg(struct JobReturn *jret);
#endif
