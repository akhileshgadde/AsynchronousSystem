#include <linux/linkage.h>
#include <linux/moduleloader.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include "sys_job.h"
#include "sys_netlink.h"

asmlinkage extern long (*sysptr)(void *arg);

#define MAX_JOBS 4
static int jobcnt = 0; /** Check if the job id exists (not imp) **/
static int curr = 0; // Variable to hold the current number of jobs in the job queue
struct mutex mut_lock; // Mutex that protects the producer and consumer job queue handling
static struct task_struct *consume_thread = NULL; // Consumer thread 

static int p_throttle_flag = 0;
static int c_sleep_flag = 0;
struct job_queue jobs_list;

static DECLARE_WAIT_QUEUE_HEAD(producer_waitq);
static DECLARE_WAIT_QUEUE_HEAD(consumer_waitq);

/** Should remove this below later : for debugging **/
void printJobQ(void);

/* submit_job: Called by the system call
   Returns 0 on success or errno on failure to the user
*/
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

	//printk("Before calling the syscall\n");	
	ret = copy_from_user(&job_flags, args, sizeof(int));
	if(ret != 0)
	{
		err = -ENOMEM;
		goto out;
	}		

	if(job_flags == 0) // Job Processing
	{ 
		kjob = copy_job_from_user(args);
		if(!kjob)
		{
			err = -ENOMEM; /** can change **/
			goto out;
		}
		
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
		
		err = sys_submitjob(jobinfo, sizeof(struct JobInfo));
	}

	out:
		if(jobinfo)
			kfree(jobinfo);
		
		return err;
}

/* copy_job_from_user: To copy Job details from the user land
   Return struct kJob pointer on success 
   On failure, returns error pointer
*/
struct kJob* copy_job_from_user(void *args)
{
	int err = 0;
	struct Job *job = NULL;
	struct kJob *kjob = NULL;
	struct filename *uinp_file = NULL, *uout_file = NULL;

	//printk("In copy job from user\n");	
	job = (struct Job*) kmalloc(sizeof(struct Job), GFP_KERNEL);
	if(!job)
	{
		err = -ENOMEM;
		goto out;
	}

	memset(job, 0, sizeof(struct Job));
	
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

	memset(kjob, 0, sizeof(struct kJob));
	
	kjob->job_id = ++jobcnt;
	
	kjob->job = (struct Job*) kmalloc(sizeof(struct Job), GFP_KERNEL);
	if(!kjob->job)
	{
		err = -ENOMEM;
		goto out;
	}
	
	memset(kjob->job, 0, sizeof(struct Job));

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

	memset(kjob->job->input_file, 0, sizeof(kjob->job->input_file));

	strcpy(kjob->job->input_file, uinp_file->name);
	kjob->job->input_file[strlen(uinp_file->name)] = '\0';

	kjob->job->output_file = (char*) kmalloc(strlen(uout_file->name)+1, GFP_KERNEL);
    if(!(kjob->job->output_file))
    {
        err = -ENOMEM;
        goto out;
    }

	memset(kjob->job->output_file, 0, sizeof(kjob->job->output_file));
    
	strcpy(kjob->job->output_file, uout_file->name);
    kjob->job->output_file[strlen(uout_file->name)] = '\0';

	kjob->job->key = (char*) kmalloc((job->keylen)+1, GFP_KERNEL);
    if(!(kjob->job->key))
    {
        err = -ENOMEM;
        goto out;
    }

	memset(kjob->job->key, 0, sizeof(kjob->job->key));
    
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
		freeJob(kjob);

	return kjob;
}

/* freeJob: Will deallocate the memory for the kJob in kernel */
void freeJob(struct kJob *kjob)
{
	if(kjob)
	{
		//printk("Freeing memory for kjob\n");
		if(kjob->job)
		{
			//printk("Freeing memory for kjob->job\n");
			if(kjob->job->input_file)
				kfree(kjob->job->input_file);
			if(kjob->job->output_file)
                kfree(kjob->job->output_file);
			if(kjob->job->key)
                kfree(kjob->job->key);
			kfree(kjob->job);		
		}
		
		kfree(kjob);
		//kjob = NULL;	
	}
	
}

/* copy_jobinfo_from_user: To copy JobInfo details from the user land
   Return sruct JobInfo pointer on success 
   On failure, returns error pointer
*/
struct JobInfo* copy_jobinfo_from_user(void *args)
{
	int err = 0;
	struct JobInfo *jobinfo = NULL;
	struct JobQInfo *jobq = NULL;

	//printk("In copy job info from user\n");	
	jobinfo = (struct JobInfo*) kmalloc(sizeof(struct JobInfo), GFP_KERNEL);
	if(!jobinfo)
	{
		err = -ENOMEM;
		goto out;
	}

	memset(jobinfo, 0, sizeof(struct JobInfo));
		
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

		memset(jobq, 0, sizeof(struct JobQInfo));
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
	}	

	return jobinfo;
}

/* getJobsFromQueue: Copies all the jobs in the job queue to the user land
   Returns 0 on success and errno on failure
*/
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

	//printk("In copy jobs from queue\n");	
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
		
		memset(jobdesc, 0, sizeof(struct JobDesc));	
		jobdesc->job_id = kjob->job_id;
		jobdesc->job_type = kjob->job->job_type;
		jobdesc->priority = kjob->job->priority;
		
		err = copy_to_user(jobinfo->jobq->jobs_arr[i], jobdesc, sizeof(struct JobDesc));
        if(err != 0)
        {
            err = -EINVAL;
            goto out;
        }
		
		if(jobdesc)
			kfree(jobdesc);
		
		i++;
		
		if(i == maxcnt)
			break;	
	}
	
	err = copy_to_user(&(jobinfo->jobq->job_cnt), &maxcnt, sizeof(int));
				
out:
	mutex_unlock(&mut_lock);
	if(err < 0)
	{
		if(jobdesc)
			kfree(jobdesc);
	}	
	
	return err;
}

/* findJobInQueue: Finds the job based on the job id in the job queue
   Returns the job queue node on success
   Returns NULL if the job is not present in the queue
*/
struct job_queue* findJobInQueue(int jobid)
{
    struct list_head *pos;
    struct job_queue *temp;
    struct kJob *kjob;
    struct job_queue *ret = NULL;

	//printk("In find jobs in the queue\n");
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

/* removeAllJobs: Removes all the jobs from the queue
   Returns 0
*/
int removeAllJobs(void)
{
	struct list_head *pos, *temp_node;
    struct job_queue *temp = NULL;

	//printk("In remove all jobs from the queue\n");
	mutex_lock(&mut_lock);
	
	list_for_each_safe(pos, temp_node, &(jobs_list.list))
	{
		temp = list_entry(pos, struct job_queue, list);
		list_del(pos);
		freeJob((struct kJob*) temp->work);
		kfree(temp);
	}
	
	curr = 0;	
	mutex_unlock(&mut_lock);
	return 0;		
}

/* removeSingleJob: Finds job in the queue and removes it
   Returns 0 on success
   Return -ENOENT if the job is not present in the queue
*/
int removeSingleJob(int jobid)
{
	int ret = 0;
	struct job_queue *job = NULL;

	//printk("In remove single job from the queue\n");	
	mutex_lock(&mut_lock);
	job = findJobInQueue(jobid);
	if(!job)
	{
		printk("Job is not in the queue.\n");
		ret = -EINVAL;
		goto out;
	}
	
	list_del(&(job->list));
	freeJob((struct kJob*) job->work);
	kfree(job);
	
	curr--;
out:
	mutex_unlock(&mut_lock);
	return ret;
}

/* changeJobPriorityInQueue: Finds job in the queue and changes it priority
   Returns 0 on success
   Return -ENOENT if the job is not present in the queue
*/
int changeJobPriorityInQueue(int jobid, int priority)
{
	int ret = 0;
    struct job_queue *job = NULL;
	struct kJob *temp;

	//printk("In change job priority\n");
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

/* processJobQueueRequest: Handles job queue processing
   Returns 0 on success and errno on failure
*/
int processJobQueueRequest(struct JobInfo *jobinfo)
{
	int flags;
	int ret = 0;

	//printk("In process job queue request\n");
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

/* sys_submitjob: Handles both job processing and job queue processing
   For job processing, creates producer threads
   Returns 0 on success, errno on failure
*/
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

/* producer: Thread which puts the job in the queue
   Returns 0 on success or errno on failure
*/
int producer(void *arg)
{
	int err = 0;
	struct job_queue *temp = NULL;
	
	printk("===Producer===\n");
	mutex_lock(&mut_lock);
	//printk("Curr in producer: %d\n", curr);
	if((curr + 1) > MAX_JOBS)
	{
		printk("Producer waiting...More number of jobs in the queue\n");
		
		mutex_unlock(&mut_lock);
		wait_event_interruptible(producer_waitq, p_throttle_flag != 0);
		printk("Producer: woken up from the wait q after\n");	
		p_throttle_flag = 0;
		mutex_lock(&mut_lock);
	}
	
	temp = (struct job_queue*) kmalloc(sizeof(struct job_queue), GFP_KERNEL);
	if(!temp)
	{
		err = -ENOMEM;
		goto out;
	}

	memset(temp, 0, sizeof(struct job_queue));
	
	temp->work = arg;
	
	// Adding the work to the job queue
	printk("Adding job to the work queue.\n");
	list_add_tail(&(temp->list), &(jobs_list.list));
	printJobQ();
	curr++;
	
	if(curr == 1)
	{
		printk("First job in the queue.\n");
		c_sleep_flag = 1;
        wake_up_interruptible(&consumer_waitq);
		//wake_up_process(consume_thread); 
	}
		 	
out:
	mutex_unlock(&mut_lock);
	//printk("Exiting from the producer.\n");
	return err;
}

/* consumer: Picks up the job from the queue and processes it
   Returns the status of the job to the user via netlink sockets
*/ 
int consumer(void *args)
{
	int err = 0;
	struct kJob *kjob = NULL;
	struct job_queue *temp = NULL;
	struct JobReturn *jret = (struct JobReturn *) kmalloc(sizeof(struct JobReturn), GFP_KERNEL);
    if (!jret) {
        printk("ERROR: unable to allocate memory for Jobreturn structure.\n");
        err = -ENOMEM;
        goto out;
    }
	
	printk("Consumer before sleep\n");
	msleep(40000);
	printk("Consumer got up from sleep\n");
	while(!kthread_should_stop())
	{
		printk("===Consumer===\n");
		mutex_lock(&mut_lock);
		
		temp = getHighestPriorityJob();
		if(temp != NULL) // Handling the case where all the jobs in the queue can be removed before the consumer woke up
		{
			kjob = (struct kJob*) temp->work;
			printk("Consumer: Job details extracted Job ID: %d Job Type: %d Job Prioriy: %d Input file: %s Output file: %s\n", kjob->job_id, kjob->job->job_type, kjob->job->priority, kjob->job->input_file, kjob->job->output_file );	
		
			list_del(&(temp->list));

			strcpy(jret->input_file, kjob->job->input_file);
			jret->input_file[strlen(kjob->job->input_file)] = '\0';
			jret->job_type = kjob->job->job_type;	
		}

		if(curr != 0) // Producer puts jobs in queue, but user removed all of them before consumer got up, so curr=0 then, should not decrement
			curr--;

		if(curr < MAX_JOBS)
		{
			//printk("Consumer: There can be producers waiting\n");
            p_throttle_flag = 1;
			wake_up_interruptible(&producer_waitq);
		}
	
		//set_current_state(TASK_INTERRUPTIBLE);
		
		if(curr == 0)
		{
			printk("Consumer: No jobs in the queue, putting into wait state\n");
			//set_current_state(TASK_INTERRUPTIBLE);
			//printk("State of consumer when put to sleep: %ld\n", consume_thread->state);	
			mutex_unlock(&mut_lock);
			if(temp)
			{
				//printk("Processing the job now.\n");
				//printk("Key passed is: %s\n", kjob->job->key);
				err = processJob(kjob);
				printk("Consumer finished processing the job.\n");
				
				jret->ret = err; 
				netlink_send_msg(jret);	
				
				freeJob(kjob);			
				kfree(temp);
			}
			wait_event_interruptible(consumer_waitq, c_sleep_flag != 0);
			c_sleep_flag = 0;
			//schedule();
			printk("Consumer is woken up by the producer\n");
		}
		else
		{
			set_current_state(TASK_RUNNING);
			//printk("Consumer: There are more jobs in the queue\n");
			mutex_unlock(&mut_lock);
			if(temp)
            {
                //printk("Processing the job now.\n");
                err = processJob(kjob);
                printk("Consumer finished processing the job.\n");

				jret->ret = err; 
                netlink_send_msg(jret);

                freeJob(kjob);
                kfree(temp);
            }
		}	
		
	}

out:
	return err;
}

/* getHighestPriorityJob: Traverses the list and returns the highest priority job
   Returns NULL if there are no jobs present in the job queue 
*/
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

/* processJob: Encrypts or decrypts the file*/
int processJob(struct kJob* kjob)
{
	int ret = 0;
	int jobtype;

	printk("Consumer processing the job\n");	
	jobtype = kjob->job->job_type;
	
	if(jobtype == 0 || jobtype == 1)
		ret = encrypt_decrypt_file(kjob->job);
	
	//else if(jobtype == 2)
		//ret = concatenate 
	return ret;
}

int encrypt_decrypt_file(struct Job *job)
{
	int ret = 0, err = 0;
	char* readbuf = NULL;
	char* writebuf = NULL; // To store the encrypted or decrypted data before writing to the output file	
	struct file *in_filp = NULL, *out_filp = NULL, *temp_filp = NULL;
	char *temp_file = NULL;
	
	mm_segment_t oldfs;
    int rbytes, wbytes;	
	
	/* To create hash of the key passed */
	struct scatterlist sg;
	struct hash_desc desc;
	char hash_in[17], hash_out[17];
	
	/* Check: Invalid flags */
	if(job->job_type!= 0 && job->job_type!= 1)
	{
		printk("Invalid job_type for encryption/decryption\n");
		err = -EINVAL;
		goto out;
	}

	/* opening the input file; in case of symbolic link- it follows the path till the original file */
	in_filp = filp_open(job->input_file, O_RDONLY, 0);
	if(!in_filp || IS_ERR(in_filp))
	{
		printk("Error opening the input file\n");
		err = PTR_ERR(in_filp);
		goto out;
	}

	/* Check: Input file is a valid regular file */
	if(!S_ISREG(in_filp->f_path.dentry->d_inode->i_mode))
	{
		printk("Input file is not a regular file.\n");
		err = -EINVAL;
		goto out;
	}

	/* Check if the input file has read permissions */
	if(!(in_filp->f_op->read))
	{
		printk("Input file does not have read permissions\n");
		err = -EPERM;
		goto out;
	}
	/* opening the output file; creates output if it does not exist
	 * in case of symbolic links, it will follow the path to the original file
	 */
	out_filp = filp_open(job->output_file, O_WRONLY | O_CREAT, 0770);
	if(!out_filp || IS_ERR(out_filp))
	{
		printk("Outfile could not be opened for writing\n");
		err = PTR_ERR(out_filp);
		goto out;
	}

	/* Check: Output file is a valid regular file */
	if(!S_ISREG(out_filp->f_path.dentry->d_inode->i_mode))
	{
		printk("Output file is not a regular file.\n");
		err = -EINVAL;
		goto out;
	}

	/* Check if the output file has write permissions */
	if(!(out_filp->f_op->write))
	{
		printk("Output file does not have write permissions\n");
		err = -EPERM;
		goto out;
	}

	/* Check: Both files are same
	 * checks for normal files, symbolic and hard links
	 */
	if(in_filp->f_path.dentry->d_inode->i_ino == out_filp->f_path.dentry->d_inode->i_ino)
	{
		if(in_filp->f_path.dentry->d_sb->s_root->d_inode->i_ino == out_filp->f_path.dentry->d_sb->s_root->d_inode->i_ino)
		{
			printk("Both input and output files are the same.\n");
			err = -EINVAL;
			goto out;
		}
	}		
	
	/* Truncating the output file */
	ret = vfs_truncate(&out_filp->f_path, 0);
	if(ret != 0)
	{
		printk("Error truncating the output file.\n");
		err = ret;
		goto out;
	}

	/* allocating memory for the read buffer */
	readbuf = (char*) kmalloc(PAGE_SIZE, GFP_KERNEL); //PAGE_SIZE
	if(!readbuf)
	{
		printk("Error allocating memory for the read buffer\n");
		err = -ENOMEM;
		goto out;
	}
	memset(readbuf, 0, sizeof(readbuf));

	/* allocating memory for the write buffer */
	writebuf = (char*) kmalloc(PAGE_SIZE, GFP_KERNEL); //PAGE_SIZE
	if(!writebuf)
	{
		printk("Error allocating memory for the write buffer\n");
		err = -ENOMEM;
		goto out;
	}
	memset(writebuf, 0, sizeof(writebuf));

	/* Creating hash of the key passed to the kernel */	
	sg_init_one(&sg, job->key, job->keylen);
	desc.tfm = crypto_alloc_hash("md5", 0, CRYPTO_ALG_ASYNC);
	if(!desc.tfm)
	{
		printk("Error allocating memory for hash in the kernel\n");
		err = -ENOMEM;
		goto out;
	}

	if((crypto_hash_digest(&desc, &sg, sg.length, hash_in)) != 0)
	{
		printk("Error generating hash for the key\n");
		err = -EINVAL; // *** check if there is a better error ***
		crypto_free_hash(desc.tfm);
		goto out;
	}

	crypto_free_hash(desc.tfm);
	hash_in[16] = '\0';

	/* ==== Opening a temporary output file ==== */
	temp_file = (char*) kmalloc(strlen(job->output_file)+5, GFP_KERNEL);
	if(!temp_file)
	{
		printk("Error allocating memory for the temporary file buffer\n");
		err = -ENOMEM;
		goto out;
	}

	memset(temp_file, 0, sizeof(temp_file));
	strcpy(temp_file, job->output_file);
	strcpy(temp_file+strlen(job->output_file), ".tmp");
	temp_file[strlen(temp_file)] = '\0';

	temp_filp = filp_open(temp_file, O_WRONLY | O_CREAT | O_TRUNC, 0770);
	if(!temp_filp || IS_ERR(temp_filp))
	{
		printk("Error opening the temporary file\n");
		err = PTR_ERR(temp_filp);
		goto out;
	}

	/* ==== Reading data from input file (encrypting/decrypting) to temporary file ==== */
    in_filp->f_pos = 0;

    oldfs = get_fs();
    set_fs(KERNEL_DS);

	if(job->job_type == 0) // encryption
	{
		/* The created hash should be copied into the output file before encrypted data. */
		wbytes = temp_filp->f_op->write(temp_filp, hash_in, 17, &temp_filp->f_pos);
		if(wbytes == 0)
		{
			printk("Error while writing key to the temporary file.\n");
			/* Unlinking the temp and output file */
			ret = unlink_files(temp_filp, out_filp);
			err = (ret != 0) ? ret : -EINVAL;
			goto out;
		}

		while((rbytes = in_filp->f_op->read(in_filp, readbuf, PAGE_SIZE, &in_filp->f_pos)) > 0)
		{
				/* Encrypt the input data */
			if((func_encrypt_decrypt(hash_in, 16, writebuf, rbytes, readbuf, rbytes, job->job_type)) != 0)
			{
				printk("Error while encrypting data.\n");
				/* Unlinking the temp and output file */
				ret = unlink_files(temp_filp, out_filp);
				err = (ret != 0) ? ret : -EINVAL;
				goto out;
			}

			/* Writing to temp out file first */
			wbytes = temp_filp->f_op->write(temp_filp, writebuf, rbytes, &temp_filp->f_pos);

			if(wbytes == 0)
			{
				printk("Error while writing encrypted data to the temporary file.\n");
				/* Unlink both the temp and output files */
				ret = unlink_files(temp_filp, out_filp);
				err = (ret != 0) ? ret : -EINVAL;
				goto out;
			}

			if(rbytes < PAGE_SIZE)
			    break;
        }
    }
	
	else if(job->job_type == 1) // decryption
    {
		/* Get the hash that is in the output file */
		rbytes = in_filp->f_op->read(in_filp, hash_out, 17, &in_filp->f_pos);
		hash_out[16] = '\0';
		//printk("HASH 2: %s\n", hash_out);

		if(strcmp(hash_in, hash_out) != 0)
		{
			printk("The key is not symmetric\n");
			/* Unlink the temporary and output files */
			ret=unlink_files(temp_filp, out_filp);
			err = (ret != 0) ? ret : -EINVAL;
			goto out;
		}

		while((rbytes = in_filp->f_op->read(in_filp, readbuf, PAGE_SIZE, &in_filp->f_pos)) > 0)
		{
			/* Decrypting the data */
			if((func_encrypt_decrypt(hash_in, 16, writebuf, rbytes, readbuf, rbytes, job->job_type)) != 0)
			{
				printk("Error while decrypting data.\n");
				ret = unlink_files(temp_filp, out_filp);
				err = (ret != 0) ? ret : -EINVAL;
				goto out;
			}

			/* Writing to temp out file first */
			wbytes = temp_filp->f_op->write(temp_filp, writebuf, rbytes, &temp_filp->f_pos);
			if(wbytes == 0)
			{
				printk("Error while writing decrypted data to the output file.\n");
				ret = unlink_files(temp_filp, out_filp);
				err = (ret != 0) ? ret : -EINVAL;
				goto out;
			}

			if(rbytes < PAGE_SIZE)
				break;
		}
	}

	/* Write to temporary file succeeded
	 * Rename the temporary file to required output file
	 */
	if((vfs_rename(temp_filp->f_path.dentry->d_parent->d_inode, temp_filp->f_path.dentry, out_filp->f_path.dentry->d_parent->d_inode, out_filp->f_path.dentry, NULL, 0)) != 0)
	{
		printk("Error while renaming temporary file to output file\n");
		ret = unlink_files(temp_filp, out_filp);
		err = (ret != 0) ? ret : -EINVAL;
		goto out;
	}

	set_fs(oldfs);

out:
	if(readbuf)
        kfree(readbuf);
    if(writebuf)
		kfree(writebuf);
	if(in_filp && !IS_ERR(in_filp))
		filp_close(in_filp, NULL);
	if(out_filp && !IS_ERR(out_filp))
		filp_close(out_filp, NULL);
	if(temp_file)
		kfree(temp_file);
	if(temp_filp && !IS_ERR(temp_filp))
		filp_close(temp_filp, NULL);	
	
	return err;
}

/* Function, unlink_files: unlinks the temporary and output files
 * Return value : 0 if unlink succeeded, else error value
 */
int unlink_files(struct file *temp_filp, struct file *out_filp)
{
	int err = 0, ret;
	
	/* Unlinking temporary file */
	ret = vfs_unlink(temp_filp->f_path.dentry->d_parent->d_inode, temp_filp->f_path.dentry, NULL);
	if(ret < 0)
	{
		printk("Error while unlinking the temporary file.\n");
		err = ret;
	}
	
	/* Unlinking output file */
	ret = vfs_unlink(out_filp->f_path.dentry->d_parent->d_inode, out_filp->f_path.dentry, NULL);
	if(ret < 0)
	{
		printk("Error while unlinking the output file.\n");
		err = ret;
	}

	return err;
}

/* Function, func_encrypt_decrypt: Encrypts/Decrypts data
 * Inputs: Cipher key, key length, destination buffer, destination length, source buffer, source length, flags
 * Encrypt if flags = 0; decrypt if flags = 1
 */
static int func_encrypt_decrypt(char *key, int keylen, char* dest, size_t dest_len, char* src, size_t src_len, int flags)
{
	int err = 0;
	struct scatterlist sg_in[1];
	struct scatterlist sg_out[1];
	struct crypto_blkcipher *tfm = NULL;
	struct blkcipher_desc desc;
	int ret;
	u8 *iv = "AABBCCDDEEFFGGHH";

	/* Allocating the cipher */
	tfm =  crypto_alloc_blkcipher("ctr(aes)", 0, CRYPTO_ALG_ASYNC);

	if(IS_ERR(tfm))
	{
			printk("Error allocating memory for tfm object\n");
			err = PTR_ERR(tfm);
			goto out;
	}

	desc.tfm = tfm;

	/* Setting the key */
	ret = crypto_blkcipher_setkey((void*)tfm, key, keylen);
	if(ret < 0)
	{
		printk("Error setting the key\n");
		err = -EINVAL;
		goto out;
	}

	/* Setting the IV for encryption/ decryption */
	crypto_blkcipher_set_iv(tfm, iv, 16);

	sg_init_table(sg_in, 1);
	sg_set_buf(sg_in, src, src_len);

	sg_init_table(sg_out, 1);
	sg_set_buf(sg_out, dest, dest_len);

	if(flags == 0) // Encrypt
	{
		desc.flags = 0;
		ret = crypto_blkcipher_encrypt(&desc, sg_out, sg_in, src_len);
		if(ret < 0)
		{
			printk("Error encrypting blocks\n");
			err = -EINVAL;
			goto out;
		}
	}

	else if(flags == 1) // Decrypt
	{
		ret = crypto_blkcipher_encrypt(&desc, sg_out, sg_in, src_len);
		if(ret < 0)
		{
			printk("Error encrypting blocks\n");
			err = -EINVAL;
			goto out;
		}
	}

	out:
		if(!IS_ERR(tfm))
			kfree(desc.tfm);
	return err;
}

/* Receives the first message from the user application 
*  on the netlink socket and records the process-id of 
*  the sending process in a global pid variable. 
*  It is used to send message later to the correct process.
*/
void netlink_recv_msg(struct sk_buff *skb)
{
    struct nlmsghdr *nlh = NULL;

    printk(KERN_INFO "Entering: %s\n", __FUNCTION__);
    
	nlh = (struct nlmsghdr *)skb->data;
    printk(KERN_INFO "Netlink received msg payload: %s\n", (char *)nlmsg_data(nlh));
    pid = nlh->nlmsg_pid;
	return;
}

/* To send the job processing status on the netlink socket to the user application.
*  sends job_type, input_file, job processing status
*/
void netlink_send_msg(struct JobReturn *jret)
{
    struct nlmsghdr *nlh = NULL;
    struct sk_buff *skb_out = NULL;
    int msg_size;
    int res;
    
    msg_size = sizeof(struct JobReturn);
    skb_out = nlmsg_new(msg_size, 0);//No need to free, nlmsg_unicast takes care of it. :)
    if (!skb_out) {
        printk(KERN_ERR "Failed to allocate new skb\n");
        return;
    }
    nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);
    NETLINK_CB(skb_out).dst_group = 0; /* not in mcast group */
    memcpy(nlmsg_data(nlh), jret, msg_size);
    
	printk("INFO: Sending Job processing status to user on netlink socket \
			for file: %s\n", ((char *) ((struct JobReturn *) nlmsg_data(nlh))->input_file));
    
	res = nlmsg_unicast(nl_sk, skb_out, pid);
    if (res < 0) {
        printk(KERN_INFO "Error while sending back to user: %d\n", res);
    }
    return;
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

		printk("Job details in the queue; Job ID: %d Job Type: %d Priority: %d Input File: %s Output File:%s\n", kjob->job_id, kjob->job->job_type, kjob->job->priority, kjob->job->input_file, kjob->job->output_file);
				
	}
}

/* Initializes the mutex: mut_lock for handling producer and consumer job queue */
/* The consumer thread is started; [For now assume there is only one consumer] */
/* Initializes the job queue */
/* Netlink socket creation */
static int __init init_sys_submitjob(void)
{
	int err = 0;
	struct netlink_kernel_cfg cfg = {
		.input = netlink_recv_msg,
    };	
	
	if (sysptr == NULL)
			sysptr = submit_job;

	mutex_init(&mut_lock);
       
	/* Net link socket initialization */
    //printk("INIT: Creating netlink socket.\n");
    nl_sk = netlink_kernel_create(&init_net, NETLINK_USER, &cfg);
    if (!nl_sk) {
        printk(KERN_ERR "%s: Netlink socket creation failed.\n", __FUNCTION__);
        err = -ENOMEM;
        goto out;
    }

	INIT_LIST_HEAD(&jobs_list.list);
 
	consume_thread = kthread_create(consumer, NULL, "consumer");
	if(!consume_thread || IS_ERR(consume_thread))
		err = PTR_ERR(consume_thread); // need to change
	
	printk("installed new sys_submitjob module\n");
out:
	return err;
}

/* Terminates the consumer thread */
static void  __exit exit_sys_submitjob(void)
{
	/** mutex destroy ?? **/
	//printk("Exiting module.\n");
	if (sysptr != NULL)
	{
		if(consume_thread)
		{
			kthread_stop(consume_thread);
			consume_thread = NULL;
		}	
		sysptr = NULL;
	}
	
	/* Releasing netlink socket */
	//printk("releasing netlink socket.\n");
	if (nl_sk)
		netlink_kernel_release(nl_sk);
	
	printk("removed sys_submitjob module\n");
}	

module_init(init_sys_submitjob);
module_exit(exit_sys_submitjob);
MODULE_LICENSE("GPL");
