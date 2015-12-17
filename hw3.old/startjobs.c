//#include <asm/unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
//#include <asm/types.h>
//#include <linux/types.h>
//#include <sys/socket.h>
//#include <linux/netlink.h>
#include "job.h"
#include "usr_netlink.h"
//#include "usr_signal.h"

#ifndef __NR_submit_job
#error submit_job system call not defined
#endif

int main(int argc, const char *argv[])
{
	int ret = 0;
	void *data;
	//int send_net_flag = 1;
	//int sockfd;
	int option = 1;
	//char choice;
	pthread_t t_netlink;
	int pid = getpid();
	printf("main: pid: %d\n", pid);
	
	if (pthread_create(&t_netlink, NULL, netlink_process, (void *) &pid) != 0) {
		printf("Thread creation error.\n");
		ret = -errno;
		goto out;
	}
	
	while (option) {
		printf("option: %d\n", option);
	    /* signals related stuff */
    	set_sigusr1_signal();

		data = getUserInput(&ret);
		if(ret < 0)
		{
			printf("Error processing the user input.\n");
			exit(EXIT_FAILURE);
		} 
		ret = syscall(__NR_submit_job, data);
		if (ret == 0)
			printf("syscall returned %d\n", ret);
		else
			printf("syscall returned %d (errno=%d)\n", ret, errno);
		
		#if 0
		sockfd = create_netlink_sockfd();
		if (sockfd < 0) {
			printf("Nelink socket creation failed.\n");
			ret = sockfd;
			goto out;
		}
		if (send_net_flag) {
			ret = send_netlink_message(sockfd);
			if (ret < 0) {
				printf("NETLINK: Sending message to kernel failed.\n");
				goto out;
			}
			send_net_flag = 0;
		}
		#if 0
		ret = fcntl(sockfd, F_SETFL, O_NONBLOCK);
		if (ret < 0) {
			printf("Setting socket to non-blocking failed.\n");
			goto out;
		}
		#endif
		/* code to check if any messages received on socket */
		ret = receive_netlink_message(sockfd);
		if (ret == 1) {
			close(sockfd);
			goto out;
		}
		else if (ret < 0) {
			printf("NETLINk sockfd receive error: %d\n", ret);
			goto out;
		
		}
		printf("Do you want to submit more jobs(Y/y for Yes and N/n for No).\n");
		getchar();
		scanf("%c", &choice);
		getchar();
		printf("choice: %c\n", choice);
		if ((choice == 'n') || (choice == 'N'))
			option = 0;
		#endif
	}
	#if 0
	//write code to do some random work and keep checking netlink socket.
	if (option == 0) {
		printf("Out of while loop. calling do_random_work.\n");
		do_random_work(sockfd);
	}
	close(sockfd);
	#endif
	return 0;
out:
	return ret;
}

void do_random_work(int sockfd)
{
	int incr = 0;
	int ret; 
	while (1)
	{
		incr++;
		if (incr == 32765)
			incr = 0;
	}
}
