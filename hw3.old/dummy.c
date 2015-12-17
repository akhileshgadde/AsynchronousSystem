// Limit the maximum jobs that can be in the queue (max)
// Have a global current variable

// Data structures needed: 1. Job Queue; 2. Producer WaitQ 3. Consumer WaitQ  

// int wake_up_process(struct task_struct *p); : can be used to wake up the consumer/producer threads
// To stop a thread: int kthread_stop(struct task_struct *p); : will give segfault if thread is not there. This will not terminate the thread, but will wait for the thread to terminate on itself.

#include <linux/sched.h> //struct task_struct
#include <linux/kthread.h> //thread APIs
#include <linux/mutex.h> //locks 


#define MAX_JOBS 5 // actually should depend on the number of CPU cores.. how many?
static int curr = 0;
static struct mutex *mut_lock;

int sys_submit(void *args, int len)
{
	struct task_struct *producer_thread = NULL;
	char thread_name[] = "producer";
	if (len != 16)
	{
		// call producer with the job
		// Can use kthread_run [which will create and wake up the producer thread; no need to call wake_up_process again]
		producer_thread = kthread_run(producer, (void*) kjob, thread_name); 
		if(!producer_thread || IS_ERR(producer_thread))
		{
			err = -EINVAL; // should find a better one
			goto out;
		}
	}
}

int producer(void *job)
{
	int err = 0;
	// Need to have a queue - linked list to add jobs
	// List node variable declaration
	mutex_lock(mut_lock);
	if(curr > max)
	{
		// put the producer to a Producer's WaitQ
		//add_to_producer_waitq(...);
	}

	// Allocate memory for the list node
	// Assign values
	// add to queue	
	curr++;
	if(curr == 1)
		//wakeup consumer(s) from Consumer WaitQ
out:
	mutex_unlock(mut_lock);
	return err;
}

int consumer()
{
	struct kJob *kjob;
	// List node variable declaration
	
	mutex_lock(mut_lock);
	if(curr == 0)
	{
		// No jobs in the queue
		// put consumer to sleep
		// add the consumer to consumerWaitQ
		unlock(mut_lock);
		goto out;	
	}	
	
	// Remove the element from the queue
	curr--;
	if(curr == 0)
		// put thread to sleep
		// add to consumerWaitQ
	
	mutex_unlock(mut_lock);
	
	// process the request
	// free the list node;	
out:
	return err;		
}

#if 0
int add_to_producer_waitq(..)
{
	// grab a spin lock to add to the waitq
	//insert into the list	
}
#endif

/* Need to initialize the mutex for handling producer and consumer job queue here */
/* Also, start the consumer thread; For now assume there is only one consumer */
static int __init init_sys_submitjob(void)
{
		int err = 0
		struct task_struct *consume_thread = NULL;
		
        printk("installed new sys_submitjob module\n");
        if (sysptr == NULL)
                sysptr = submit_job;

		mutex_init(mut_lock);
		
		consume_thread = kthread_create(consumer, NULL, "consumer");
		if(!consume_thread || IS_ERR(consume_thread))
			err = -EINVAL; // need to change
			
        return err;
}
