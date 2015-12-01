#ifndef __STARTJOBS_H__
#define __STARTJOBS_H__

#include "common.h"

struct Job* createJob(int, const char*, const char*, const char*, int);
struct JobInfo* createJobInfo(int, int, int, struct JobQInfo*);
//void* getUserInput(int*);
void* processEnDecryptReq(int);
//void* processRemoveReq(void);
void* processListReq(int);
void* processGetNumJobs(void);
//void* processChangePriorityReq(void);
#endif
