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

#define MAX_JOBS 1
static int jobcnt = 0; /** Check if the job id exists (not imp) **/
static int curr = 0; // Variable to hold the current number of jobs in the job queue
struct mutex mut_lock; // Mutex that protects the producer and consumer job queue handling
static struct task_struct *consume_thread = NULL; // Consumer thread 
int first = 0;

struct job_queue jobs_list;

//wait_queue_head_t producer_waitq;

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

	printk("Job flags: %d\n", job_flags);
	
	if(job_flags == 0) // Job Processing
	{ 
		kjob = copy_job_from_user(args);
		if(!kjob)
		{
			err = -ENOMEM; /** can change **/
			goto out;
		}
		
		printk("Job processing :%d %d\n", kjob->job_id, kjob->job->job_type);	
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
		printk("Job queue processing flags:%d\n", jobinfo->flags);
		
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
	//copy_from_user(kjob->job, args, size );
	
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

out:
	if(err < 0)
	{
		if(jobinfo)
			kfree(jobinfo);
		jobinfo = NULL;
	}	

	return jobinfo;
}

int sys_submitjob(void* args, int len) // use argslen to check the buffers passed by user
{
	int err = 0;
	struct task_struct *produce_thread = NULL;
	char threadname[10] = "producer";
	
	threadname[8] = jobcnt + '0'; //need to change to curr
	threadname[9] = '\0';
	
	printk("Thread name: %s\n", threadname);
	
	printk("In sys_submitjob %d\n", len);
	
	if(len == 16)
	{
		printk("Processing job queue.\n");
	}
	
	else
	{
		printk("Processing job.\n");
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
	//int mutex_flag = 0;
	
	printk("===Producer===\n");
	mutex_lock(&mut_lock);
	printk("Curr in producer: %d\n", curr);
	if(curr > MAX_JOBS)
	{
		/** Bad implementation; need to change; add to producer waitq **/
		printk("Producer waiting...More number of jobs\n");
		//mutex_flag = 1;
		//mutex_unlock(&mut_lock);
		//wait_event_interruptible(producer_waitq, curr < MAX_JOBS);
		err = -EINVAL;
		goto out;			
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
	//if(mutex_lock == 0)
	mutex_unlock(&mut_lock);
	printk("Exiting from the producer.\n");
	return err;
}

int consumer(void *args)
{
	int err = 0;
	struct kJob *kjob;
	struct job_queue *temp = NULL;
	//struct list_head *pos;

	//msleep(50000);
	printk("===Consumer===\n");
	while(!kthread_should_stop())
	{
		mutex_lock(&mut_lock);
		printk("Consumer: there are jobs in the queue.\n");
		temp = getHighestPriorityJob();
	
		kjob = (struct kJob*) temp->work;
		printk("Consumer: Job details extracted %d %d %s %s\n", kjob->job_id, kjob->job->job_type,kjob->job->input_file, kjob->job->output_file );	
		
		list_del(&(temp->list));
		curr--;

		if(curr == 0)
		{
			printk("Putting consumer to sleep\n");
			set_current_state(TASK_INTERRUPTIBLE);
			mutex_unlock(&mut_lock);
			schedule();
		}
		else
		{
			//if(curr < MAX_JOBS)
			//	wake_up_interruptible(&producer_waitq);
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

struct job_queue* getHighestPriorityJob(void)
{
	struct list_head *pos;
	struct job_queue *temp;
	struct kJob *kjob;
	int max = -1;
	struct job_queue *ret = NULL;
	
	list_for_each(pos, &(jobs_list.list))
	{
		temp = list_entry(pos, struct job_queue, list);

		kjob = (struct kJob*) temp->work;
		
		if(kjob->job->priority > max)
		{
			max = kjob->job->priority;
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

		printk("Job details in the queue; %d %d %s %s\n", kjob->job_id, kjob->job->job_type, kjob->job->input_file, kjob->job->output_file);
				
	}

	#if 0
	/** The highest priority job in the queue **/
	temp = getHighestPriorityJob();
	kjob = (struct kJob*) temp->work;
	printk("The highest priority job in the queue is:\n");
	printk("Job details in the queue; %d %d %s %s\n", kjob->job_id, kjob->job->job_type, kjob->job->input_file, kjob->job->output_file);
	#endif
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
