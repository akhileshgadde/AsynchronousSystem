#include <asm/unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* close */
//#include <sys/types.h>
#include <errno.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include "common.h"
#include "usr_netlink.h"
#include <signal.h>

struct sockaddr_nl src_addr, dest_addr;
struct nlmsghdr *nlh = NULL;
int sockfd;
struct msghdr msg;
struct iovec iov;

int create_netlink_sockfd()
{
	sockfd = socket(AF_NETLINK, SOCK_RAW, NETLINK_USER);
    if (sockfd < 0) {
        printf("Error opening Netlink socket: %s.\n", strerror(errno));
        return -errno;
    }
	return sockfd;
}

void *netlink_process(void *args)
{
	int *pid = (int *) args;
	int ret = 0;
	int sockfd;
	//struct sigaction sa;
	sigset_t set;
	printf("Thread: pid: %d\n", *pid);

	/* blocking all signals in this thread */
	sigemptyset(&set);
	ret = pthread_sigmask(SIG_SETMASK, &set, NULL);
	if (ret) {
		printf("Error in pthread_sigmask.\n");
	}
	printf("pthread_sigmask set successfully in thread.\n");
	
	sockfd = create_netlink_sockfd();
	if (sockfd < 0){
		printf("Netlink socket creation error.\n");
		return (void *) -errno;
	}
	
	ret = send_netlink_message(sockfd, *pid);
	if (ret < 0) {
		printf("Sending message to kernel on netlink socket failed.\n");
		return (void *) -errno;
	}
	#if 1
	ret = receive_netlink_message(sockfd, *pid);
	if (ret == 0) {
		printf("Netlink connection failed on other end.\n");
		return (void *) ret;
	}
	#endif
	close(sockfd);
	return NULL;
}


int send_netlink_message(int sockfd, int pid)
{
	int ret = 0;
	ssize_t sent_bytes = 0;
	
	memset(&src_addr, 0, sizeof(src_addr));
	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = pid;
	
	bind(sockfd, (SA *) &src_addr, sizeof(src_addr));
	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = 0;
    dest_addr.nl_groups = 0;
	
	nlh = (struct nlmsghdr *) malloc(NLMSG_SPACE(MAX_PAYLOAD)); //Need to free memory
	memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
	memset(&msg, 0, sizeof(msg));
	nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
	nlh->nlmsg_pid = pid;
	nlh->nlmsg_flags = 0;
	
	strcpy(NLMSG_DATA(nlh), "Hello from user application");
	iov.iov_base = (void *) nlh;
	iov.iov_len = nlh->nlmsg_len;
	msg.msg_name = (void *)&dest_addr;
	msg.msg_namelen = sizeof(dest_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	
	printf("Sending message to kernel\n");
	sent_bytes = sendmsg(sockfd, &msg, 0);
	if (sent_bytes < 0) {
		printf("Sending message to kernel failed: %s.\n", strerror(errno));
		ret = -1;
		goto out;
	}
	printf("Sent message to kernel\n");
	#if 0
	/* Read message from kernel */
    ret = recvmsg(sockfd, &msg, 0);
    printf("Received message payload: %s, %d\n", (char *) NLMSG_DATA(nlh), ret);
	#endif

out:
	return ret;
}

int receive_netlink_message(int sockfd, int pid)
{
	int ret = 0;
	//struct JobReturn *jret = NULL;
	ssize_t recv_bytes;
	fd_set rset;
	int s_ret;
	
	printf("In receive_netlink_message.\n");
	#if 0
	jret = (struct JobReturn *) malloc(sizeof(struct JobReturn));
	if (!jret) {
		ret = -ENOMEM;
		goto out;
	}
	#endif
	for (;;)
	{
		FD_ZERO(&rset);
		FD_SET(sockfd, &rset);
		
		//memset(&msg, 0, sizeof(msg));
		s_ret = select(FD_SETSIZE, &rset, NULL, NULL, NULL);
		if (s_ret < 0) {
			printf("Select error: %s\n", strerror(errno));
			ret = -errno;
			goto out;
		}
		if (FD_ISSET(sockfd, &rset)) {
				printf("sockfd selected after select.\n");
				recv_bytes = recvmsg(sockfd, &msg, 0);
				printf("Recv_bytes: %zd\n", recv_bytes);
				if (recv_bytes == 0)  { /* conn closed on other end */
					ret = 0;
					printf("Recv_bytes zero for socket.\n");
					continue;
					//goto out;
				}
				else if (ret < 0) {
					#if 0
					if (errno == EAGAIN) {
						/* no message from kernel */
						printf("No message to read from kernel.\n");
						ret = 1;
					}
					else  // handle errors
					#endif
					ret = -errno;
				}
				else {
					ret = 1; 
				//memcpy(jret, NLMSG_DATA(nlh), sizeof(struct JobReturn));
				job_ret = *((struct JobReturn *) NLMSG_DATA(nlh));
				printf("received from kernel:\n");
				printf("%d, %s, %d\n", job_ret.job_type, job_ret.input_file, job_ret.ret);
				printf("About to call kill for SIGUSR1.\n");
			    kill(pid, SIGUSR1);
				//printf("About to print received msg.\n");
				//printf("Received message from kernel: %s\n", (char *) NLMSG_DATA(nlh));
				}
		}
	}
out:
	//if (jret)
	//	free(jret);
	printf("exiting receive_netlink: %d\n", ret);
	return ret;
}
