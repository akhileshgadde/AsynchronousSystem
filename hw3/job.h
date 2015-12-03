#ifndef __STARTJOBS_H__
#define __STARTJOBS_H__

#include "common.h"

struct Job* createJob(int, const char*, const char*, const char*, int);
struct JobInfo* createJobInfo(int, int, int, struct JobQInfo*);
void* processEnDecryptReq(int);
void* processListReq(int);
void* processGetNumJobs(void);
void do_random_work(void);
#endif
