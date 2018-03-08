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
#include "sys_netlink.h"

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

void netlink_recv_msg(struct sk_buff *skb)
{
    struct nlmsghdr *nlh = NULL;
    //struct sk_buff *skb_out = NULL;
    //int msg_size;
    //char *msg = "Hello from kernel";
    //int res;

    printk(KERN_INFO "Entering: %s\n", __FUNCTION__);
    //msg_size = strlen(msg);
    nlh = (struct nlmsghdr *)skb->data;
    printk(KERN_INFO "Netlink received msg payload: %s\n", (char *)nlmsg_data(nlh));
    pid = nlh->nlmsg_pid;
	printk("Received Pid: %d\n", pid);	
    #if 0
	skb_out = nlmsg_new(msg_size, 0);
    if (!skb_out) {
        printk(KERN_ERR "Failed to allocate new skb\n");
        return;
    }
	
    nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);
    NETLINK_CB(skb_out).dst_group = 0; /* not in mcast group */
	//NETLINK_CB(skb_out).dst_pid = pid;
	//NETLINK_CB(skb_out).groups = 0; /* not in mcast group */
	//NETLINK_CB(skb_out).pid = 0;      /* from kernel */
    strncpy(nlmsg_data(nlh), msg, msg_size);
	printk("Netlink, sending to user: %s\n", (char *) nlmsg_data(nlh));
    res = nlmsg_unicast(nl_sk, skb_out, pid);
    if (res < 0) {
        printk(KERN_INFO "Error while sending back to user\n");
		kfree_skb(skb_out);
	}
	#endif
    return;
}

void netlink_send_msg(struct JobReturn *jret)
{
    struct nlmsghdr *nlh = NULL;
	struct sk_buff *skb_out = NULL;
	int msg_size;
    //char *msg = "Hello from kernel";
	int res;	
	//msg_size = strlen(msg);
	printk("inside netlink_send_msg, pid: %d.\n", pid);
	printk("Jret: type: %d, inp: %s, ret: %d\n", jret->job_type, jret->input_file, jret->ret);
	msg_size = sizeof(struct JobReturn);
	skb_out = nlmsg_new(msg_size, 0);//No need to free, nlmsg_unicast takes care of it. :)
    if (!skb_out) {
        printk(KERN_ERR "Failed to allocate new skb\n");
        return;
    }
	nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);
    NETLINK_CB(skb_out).dst_group = 0; /* not in mcast group */
	//strncpy(nlmsg_data(nlh), msg, msg_size);
    memcpy(nlmsg_data(nlh), jret, msg_size);
	printk("Netlink, sending to user, input_file: %s\n", ((char *) ((struct JobReturn *) nlmsg_data(nlh))->input_file));
	printk("type: %d, ret: %d\n",((struct JobReturn *) nlmsg_data(nlh))->job_type, ((struct JobReturn *) nlmsg_data(nlh))->ret);
    res = nlmsg_unicast(nl_sk, skb_out, pid);
    if (res < 0) {
        printk(KERN_INFO "Error while sending back to user: %d\n", res);
        //kfree_skb(skb_out);
    }
    return;
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
	
	printk("========In syscall....Thread name: %s=========\n", threadname);
	
	if(len == 16)
	{
		printk("Processing job queue.\n");
	}
	
	else
	{
		//printk("Processing job.\n");
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
	int job_ct = 0;
	struct job_queue *temp = NULL;
	struct JobReturn *jret = (struct JobReturn *) kmalloc(sizeof(struct JobReturn), GFP_KERNEL);
	if (!jret) {
		printk("ERROR: unable to allocate memory for Jobreturn structure.\n");
		err = -ENOMEM;
		goto out;
	}
	msleep(5000);
	while(!kthread_should_stop())
	{
		printk("===Consumer===\n");
		//jret->input_file = NULL;
		//jret->output_file = NULL;
		mutex_lock(&mut_lock);
		printk("Consumer: there are jobs in the queue.\n");
		temp = getHighestPriorityJob();
		
		kjob = (struct kJob*) temp->work;
		printk("Consumer: Job details extracted %d %d %s %s\n", kjob->job_id, kjob->job->priority, kjob->job->input_file, kjob->job->output_file );	
		
		list_del(&(temp->list));
		curr--;
		
		/* allocate and fill Jobreturn struct */
		#if 0
		jret->input_file = kmalloc(strlen(kjob->job->input_file) + 1, GFP_KERNEL);
		if (!jret->input_file) 
		{
			err = -ENOMEM;
			goto err;
		}
		jret->output_file = kmalloc(strlen(kjob->job->output_file) + 1, GFP_KERNEL);
		if (!jret->output_file) 
		{
			err = -ENOMEM;
			goto err;
		}
		#endif
		strcpy(jret->input_file, kjob->job->input_file);
		jret->input_file[strlen(kjob->job->input_file)] = '\0';
		//strcpy(jret->output_file, kjob->job->output_file);
		//jret->output_file[strlen(kjob->job->output_file)] = '\0';
		jret->job_type = kjob->job->job_type;
	
		if(curr < MAX_JOBS)
		{
			printk("Consumer: There can be producers waiting\n");
            p_throttle_flag = 1;
			job_ct = MAX_JOBS - curr;
			while (job_ct > 0) 
			{
            	wake_up_interruptible(&producer_waitq);
				job_ct--;
			}
		}

		if(curr == 0)
		{
			printk("Putting consumer to sleep\n");
			set_current_state(TASK_INTERRUPTIBLE);
			mutex_unlock(&mut_lock);
			
			/* calling netlink socket */
			jret->ret = 0; /* To be replaced by output from process_function */ 
			netlink_send_msg(jret);
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
		#if 0
		if (jret->input_file)
		{
			kfree(jret->input_file);
			jret->input_file = NULL;
		}
		if (jret->output_file)
		{
			kfree(jret->output_file);
			jret->output_file = NULL;
		}
		#endif
	}
	if(jret) 
	{
		kfree(jret);
		jret = NULL;
	}
	#if 0
err:
	if (err < 0) {
		if (jret->input_file)
			kfree(jret->input_file);
		if (jret->output_file)
			kfree(jret->output_file);
	}
	#endif
out:
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
	struct netlink_kernel_cfg cfg = {
    .input = netlink_recv_msg,
	};	
	
	printk("installed new sys_submitjob module\n");
	if (sysptr == NULL)
			sysptr = submit_job;

	mutex_init(&mut_lock);
    
	/* Net link socket initialization */
	printk("INIT: Creating netlink socket.\n");
	nl_sk = netlink_kernel_create(&init_net, NETLINK_USER, &cfg);
	if (!nl_sk)	{
		printk(KERN_ERR "%s: Netlink socket creation failed.\n", __FUNCTION__);
    	err = -ENOMEM;
		goto out;
	} 
	INIT_LIST_HEAD(&jobs_list.list);
 	
	consume_thread = kthread_create(consumer, NULL, "consumer");
	if(!consume_thread || IS_ERR(consume_thread))
		err = PTR_ERR(consume_thread); // need to change

	//init_waitqueue_head(&producer_waitq);
	printk("Exiting init function without any error.\n");
out:	
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
	printk("EXIT_MODULE: releasing netlink socket.\n");
	netlink_kernel_release(nl_sk);
	printk("removed sys_submitjob module\n");
}	

module_init(init_sys_submitjob);
module_exit(exit_sys_submitjob);
MODULE_LICENSE("GPL");
