#ifndef __STARTJOBS_H__
#define __STARTJOBS_H__

#include "common.h"

struct Job* createJob(int, const char*, const char*, const char*, int);
struct JobInfo* createJobInfo(int, int, int);
void* getUserInput(int*);
void* processEnDecryptReq(int);
void* processRemoveReq(void);
void* processListReq(void);
void do_random_work(int sockfd);
#endif
