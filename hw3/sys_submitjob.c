#include <linux/linkage.h>
#include <linux/moduleloader.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include "sys_job.h"

asmlinkage extern long (*sysptr)(void *arg);

#define MAX_JOBS 2
static int jobcnt = 0; /** Check if the job id exists (not imp) **/
static int curr = 0; // Variable to hold the current number of jobs in the job queue
struct mutex mut_lock; // Mutex that protects the producer and consumer job queue handling
static struct task_struct *consume_thread = NULL; // Consumer thread 
int first = 0;
static int p_throttle_flag = 0;

struct job_queue jobs_list;

static DECLARE_WAIT_QUEUE_HEAD(producer_waitq);
/** Should remove this below later : for debugging **/
void printJobQ(void);

asmlinkage long submit_job(void *args)
{
	int ret, err = 0;
	struct kJob *kjob = NULL;
	struct JobInfo *jobinfo = NULL;
	int job_flags;
	
	if(args == NULL)
	{
		printk("Argument passed is NULL\n");
		err = -EINVAL;
		goto out;
	}
	
	ret = copy_from_user(&job_flags, args, sizeof(int));
	if(ret != 0)
	{
		err = -ENOMEM;
		goto out;
	}		

	//printk("Job flags: %d\n", job_flags);
	
	if(job_flags == 0) // Job Processing
	{ 
		kjob = copy_job_from_user(args);
		if(!kjob)
		{
			err = -ENOMEM; /** can change **/
			goto out;
		}
		
		//printk("Job processing :%d %d\n", kjob->job_id, kjob->job->job_type);	
		err = sys_submitjob(kjob, sizeof(struct kJob));
	}

	else if(job_flags == 1) // Job Queue Processing
	{
		jobinfo = copy_jobinfo_from_user(args);
		if(!jobinfo)
		{
			err = -ENOMEM; /** can change **/
			goto out;
		}
		//printk("Job queue processing flags:%d\n", jobinfo->flags);
		
		err = sys_submitjob(jobinfo, sizeof(struct JobInfo));
	}

	/** copy all other info too **/	
	out:
		/** Giving oops (in printJobQ) if kjob is freed here; may be because this function is executing before the thread exits
			have to check this; where to free now?? in consumer ?? **/
		//if(kjob) 
		//	kfree(kjob);

		if(jobinfo)
			kfree(jobinfo);
		
		return err;
}

struct kJob* copy_job_from_user(void *args)
{
	int err = 0;
	struct Job *job = NULL;
	struct kJob *kjob = NULL;
	struct filename *uinp_file = NULL, *uout_file = NULL;
	
	job = (struct Job*) kmalloc(sizeof(struct Job), GFP_KERNEL);
	if(!job)
	{
		err = -ENOMEM;
		goto out;
	}
	
	err = copy_from_user(job, args, sizeof(struct Job));
	if(err != 0)
	{
		err = -ENOMEM;
		goto out;
	}
	
	uinp_file = getname(job->input_file);
	if(!uinp_file || IS_ERR(uinp_file))
	{
		err = PTR_ERR(uinp_file);
        goto out;
	}

	uout_file = getname(job->output_file);
	if(!uout_file || IS_ERR(uout_file))
	{
		err = PTR_ERR(uout_file);
		goto out;
	}

	kjob = (struct kJob*) kmalloc (sizeof(struct kJob), GFP_KERNEL);
	if(!kjob)
	{
		err = -ENOMEM;
		goto out;
	}
	
	kjob->job_id = ++jobcnt;
	
	kjob->job = (struct Job*) kmalloc(sizeof(struct Job), GFP_KERNEL);
	if(!kjob->job)
	{
		err = -ENOMEM;
		goto out;
	}

	kjob->job->job_flags = job->job_flags;
	kjob->job->job_type = job->job_type;
	kjob->job->priority = job->priority;
	kjob->job->keylen = job->keylen;
		
	kjob->job->input_file = (char*) kmalloc(strlen(uinp_file->name)+1, GFP_KERNEL);
	if(!(kjob->job->input_file))
	{
		err = -ENOMEM;
		goto out;
	}

	strcpy(kjob->job->input_file, uinp_file->name);
	kjob->job->input_file[strlen(uinp_file->name)] = '\0';

	kjob->job->output_file = (char*) kmalloc(strlen(uout_file->name)+1, GFP_KERNEL);
    if(!(kjob->job->output_file))
    {
        err = -ENOMEM;
        goto out;
    }

    strcpy(kjob->job->output_file, uout_file->name);
    kjob->job->output_file[strlen(uout_file->name)] = '\0';

	kjob->job->key = (char*) kmalloc((job->keylen)+1, GFP_KERNEL);
    if(!(kjob->job->key))
    {
        err = -ENOMEM;
        goto out;
    }

    err = copy_from_user(kjob->job->key, job->key, job->keylen);
	if(err != 0)
	{
		err = -ENOMEM;
		goto out;
	}
	
	kjob->job->key[job->keylen] = '\0';
out:
	if(job)
		kfree(job);
	if(uinp_file && !IS_ERR(uinp_file))
		putname(uinp_file);
	if(uout_file && !IS_ERR(uout_file))
		putname(uout_file);
	
	if(err < 0)
	{
		if(kjob)
			kfree(kjob);
		kjob = NULL;
	}

	return kjob;
}

struct JobInfo* copy_jobinfo_from_user(void *args)
{
	int err = 0;
	struct JobInfo *jobinfo = NULL;
	struct JobQInfo *jobq = NULL;
	
	jobinfo = (struct JobInfo*) kmalloc(sizeof(struct JobInfo), GFP_KERNEL);
	if(!jobinfo)
	{
		err = -ENOMEM;
		goto out;
	}
	
	err = copy_from_user(jobinfo, args, sizeof(struct JobInfo));
	if(err != 0)
	{
		err = -ENOMEM;
		goto out;
	}

	if(jobinfo->flags == 2)
	{
		jobq = (struct JobQInfo*) kmalloc(sizeof(struct JobQInfo), GFP_KERNEL);
		if(!jobq)
		{
			err = -ENOMEM;
			goto out;
		}

		err = copy_from_user(jobq, jobinfo->jobq, sizeof(struct JobQInfo));
		if(err != 0)
		{
			err = -ENOMEM;
			goto out;
		}

		jobinfo->jobq = jobq;
	}

out:
	if(err < 0)
	{
		if(jobq)
			kfree(jobq);
		if(jobinfo)
			kfree(jobinfo);
		jobinfo = NULL;
	}	

	return jobinfo;
}

int getJobsFromQueue(struct JobInfo *jobinfo)
{
	int jobcnt;
	int err = 0;
	struct list_head *pos;
    struct job_queue *temp;
    struct kJob *kjob;
	struct JobDesc *jobdesc = NULL;
	int i = 0;
	int maxcnt;
	
	jobcnt = jobinfo->jobq->job_cnt;
	
	mutex_lock(&mut_lock);	
	maxcnt = (curr <= jobcnt) ? curr : jobcnt;

	list_for_each(pos, &(jobs_list.list))
    {
        temp = list_entry(pos, struct job_queue, list);

        kjob = (struct kJob*) temp->work;
		
		jobdesc = (struct JobDesc*) kmalloc (sizeof(struct JobDesc), GFP_KERNEL);
		if(!jobdesc)
		{
			err = -ENOMEM;
			goto out;
		}
		
		jobdesc->job_id = kjob->job_id;
		jobdesc->job_type = kjob->job->job_type;
		jobdesc->priority = kjob->job->priority;
		
		err = copy_to_user(jobinfo->jobq->jobs_arr[i], jobdesc, sizeof(struct JobDesc));
        if(err != 0)
        {
            err = -EINVAL;
            goto out;
        }
		
		kfree(jobdesc);
		i++;
		
		if(i == maxcnt)
			break;	
	}
	
	err = copy_to_user(&(jobinfo->jobq->job_cnt), &maxcnt, sizeof(int));
				
out:
	if(err < 0)
	{
		if(jobdesc)
			kfree(jobdesc);
	}	
	
	mutex_unlock(&mut_lock);
	return err;
}

struct job_queue* findJobInQueue(int jobid)
{
    struct list_head *pos;
    struct job_queue *temp;
    struct kJob *kjob;
    struct job_queue *ret = NULL;

    list_for_each(pos, &(jobs_list.list))
    {
        temp = list_entry(pos, struct job_queue, list);

        kjob = (struct kJob*) temp->work;

        if(kjob->job_id == jobid)
		{
			ret = temp;
			break;
		}
    }

    return ret;
}

int removeAllJobs(void)
{
	struct list_head *pos, *temp_node;
    struct job_queue *temp;

	mutex_lock(&mut_lock);
	
	list_for_each_safe(pos, temp_node, &(jobs_list.list))
	{
		temp = list_entry(pos, struct job_queue, list);
		list_del(pos);
		kfree(temp);
		temp = NULL;
	}
	
	curr = 0;	
	mutex_unlock(&mut_lock);
	return 0;		
}

int removeSingleJob(int jobid)
{
	int ret = 0;
	struct job_queue *job = NULL;
	
	mutex_lock(&mut_lock);
	job = findJobInQueue(jobid);
	if(!job)
	{
		printk("Job is not in the queue.\n");
		ret = -EINVAL;
		goto out;
	}
	
	list_del(&(job->list));
	kfree(job);
	job = NULL;
	
	curr--;
out:
	mutex_unlock(&mut_lock);
	return ret;
}

int changeJobPriorityInQueue(int jobid, int priority)
{
	int ret = 0;
    struct job_queue *job = NULL;
	struct kJob *temp;

    mutex_lock(&mut_lock);
    job = findJobInQueue(jobid);
    if(!job)
    {
        printk("Job is not in the queue.\n");
        ret = -EINVAL;
        goto out;
    }
	
	temp = (struct kJob*) job->work;	
	temp->job->priority = priority;
	
out:
	mutex_unlock(&mut_lock);
	return ret;	
}

int processJobQueueRequest(struct JobInfo *jobinfo)
{
	int flags;
	int ret = 0;

	flags = jobinfo->flags;

	if(flags == 0)
	{
		printk("Remove all jobs from the queue\n");
		ret = removeAllJobs();
	}

	else if(flags == 1)
	{
		printk("Remove one job from the queue\n");
		ret = removeSingleJob(jobinfo->job_id);
	}

	else if(flags == 2)
	{
		printk("List jobs in the queue\n");
		ret = getJobsFromQueue(jobinfo);	
	}
	
	else if(flags == 3)
	{
		printk("Change priority of a job in the queue\n");
		ret = changeJobPriorityInQueue(jobinfo->job_id, jobinfo->priority);
	}
		
	else if(flags == 4)
	{
		printk("Get job count\n");
		mutex_lock(&mut_lock);
		ret = curr;
		mutex_unlock(&mut_lock);		
	}

	return ret;
}

int sys_submitjob(void* args, int len) // use argslen to check the buffers passed by user
{
	int err = 0;
	struct task_struct *produce_thread = NULL;
	char threadname[10] = "producer";
	
	printk("========In syscall======= \n");
	
	if(len == 20)
	{
		printk("Processing job queue request\n");
		err = processJobQueueRequest((struct JobInfo*) args);
	}	
	else
	{
		printk("Processing job: %s\n", threadname);
		
		threadname[8] = jobcnt + '0'; //need to change to curr
		threadname[9] = '\0';

		/** Invoke Producer thread **/
		produce_thread = kthread_run(producer, args, threadname);
        if(!produce_thread || IS_ERR(produce_thread))
        {
			printk("Could not create the producer thread.\n");
            err = -EINVAL; // should find a better one
            goto out;
        }		
	}
	
out:
	return err;
}

int producer(void *arg)
{
	int err = 0;
	struct job_queue *temp;
	
	printk("===Producer===\n");
	mutex_lock(&mut_lock);
	printk("Curr in producer: %d\n", curr);
	if((curr+1) > MAX_JOBS)
	{
		printk("Producer waiting...More number of jobs\n");
		
		mutex_unlock(&mut_lock);
		wait_event_interruptible(producer_waitq, p_throttle_flag != 0);
		printk("Producer: woken up after waiting\n");	
		
		mutex_lock(&mut_lock);
		p_throttle_flag = 0;
	}
	
	temp = (struct job_queue*) kmalloc(sizeof(struct job_queue), GFP_KERNEL);
	if(!temp)
	{
		err = -ENOMEM;
		goto out;
	}

	temp->work = arg;
	
	// Adding the work to the job queue
	printk("Adding to the work queue.\n");
	list_add_tail(&(temp->list), &(jobs_list.list));
	printJobQ();
	curr++;
	
	if(curr == 1)
	{
		printk("First job in the queue.\n");
		wake_up_process(consume_thread); 
	}
		 	
out:
	mutex_unlock(&mut_lock);
	printk("Exiting from the producer.\n");
	return err;
}

int consumer(void *args)
{
	int err = 0;
	struct kJob *kjob;
	//int job_ct = 0;
	struct job_queue *temp = NULL;

	msleep(50000);
	while(!kthread_should_stop())
	{
		printk("===Consumer===\n");
		mutex_lock(&mut_lock);
		printk("Consumer: there are jobs in the queue.\n");
		temp = getHighestPriorityJob();
		if(temp != NULL) // Handling the case where all the jobs in the queue can be removed before the consumer woke up
		{
			kjob = (struct kJob*) temp->work;
			printk("Consumer: Job details extracted %d %d %s %s\n", kjob->job_id, kjob->job->priority, kjob->job->input_file, kjob->job->output_file );	
		
			list_del(&(temp->list));
		}

		if(curr != 0) // Producer put jobs in queue, but user removed all of them before consumer got up, so curr=0 then, should not decrement
			curr--;

		if(curr < MAX_JOBS)
		{
			printk("Consumer: There can be producers waiting\n");
            p_throttle_flag = 1;
			wake_up_interruptible(&producer_waitq);
			#if 0
			job_ct = MAX_JOBS - curr;
			while (job_ct > 0) 
			{
            	wake_up_interruptible(&producer_waitq);
				job_ct--;
			}
			#endif
		}

		if(curr == 0)
		{
			printk("Putting consumer to sleep\n");
			set_current_state(TASK_INTERRUPTIBLE);
			mutex_unlock(&mut_lock);
			schedule();
		}	
		else
		{
			printk("Consumer: more jobs in the queue.\n");
			mutex_unlock(&mut_lock);
        }
		
		// *********process the job	*************
		
		if(temp)
		{
			kfree(temp);
			temp = NULL;
		}
	}

	return err;
}

/* Limitation: max priority that can be assigned is 256. Put a condition in user code accordingly */
struct job_queue* getHighestPriorityJob(void)
{
	struct list_head *pos;
	struct job_queue *temp;
	struct kJob *kjob;
	int min = 257;
	struct job_queue *ret = NULL;
	
	list_for_each(pos, &(jobs_list.list))
	{
		temp = list_entry(pos, struct job_queue, list);

		kjob = (struct kJob*) temp->work;
		
		if(kjob->job->priority < min)
		{
			min = kjob->job->priority;
			ret = temp;
		}
	}

	return ret; 		
}

void printJobQ(void)
{
	struct list_head *pos; 
	struct job_queue *temp;
	struct kJob *kjob;
 
	list_for_each(pos, &(jobs_list.list))
	{
		temp = list_entry(pos, struct job_queue, list);
		
		kjob = (struct kJob*) temp->work;

		printk("Job details in the queue; %d %d %s %s\n", kjob->job_id, kjob->job->priority, kjob->job->input_file, kjob->job->output_file);
				
	}
}

/* Initializes the mutex: mut_lock for handling producer and consumer job queue */
/* The consumer thread is started; [For now assume there is only one consumer] */
/* Initializes the job queue */
/* Initializes the producer's wait Q */
static int __init init_sys_submitjob(void)
{
	int err = 0;
	
	printk("installed new sys_submitjob module\n");
	if (sysptr == NULL)
			sysptr = submit_job;

	mutex_init(&mut_lock);
       
	INIT_LIST_HEAD(&jobs_list.list);
 
	consume_thread = kthread_create(consumer, NULL, "consumer");
	if(!consume_thread || IS_ERR(consume_thread))
		err = PTR_ERR(consume_thread); // need to change

	//init_waitqueue_head(&producer_waitq);
		
	return err;
}

/* Terminates the consumer thread */
static void  __exit exit_sys_submitjob(void)
{
	/** mutex destroy ?? **/
	if (sysptr != NULL)
	{
		/** Some issue here: kernel oops **/
		if(consume_thread)
		{
			kthread_stop(consume_thread);
			consume_thread = NULL;
		}	
		sysptr = NULL;
	}
	printk("removed sys_submitjob module\n");
}	

module_init(init_sys_submitjob);
module_exit(exit_sys_submitjob);
MODULE_LICENSE("GPL");
