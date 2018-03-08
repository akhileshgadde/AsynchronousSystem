/* Responsible for creating, sending and receiving on the netlink sockets. 
*  Messages from the kernel are handled by a thread created at program execution.
*  All the signals are masked in the thread using pthread_sigmask().
*  Generated SIGUSR1 when a message is received from the kernel.
*/

#include <asm/unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* close */
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
        printf("ERROR: Netlink socket cannot be opened: %s.\n", strerror(errno));
        return -errno;
    }
	return sockfd;
}

void *netlink_process(void *args)
{
	int *pid = (int *) args;
	int ret = 0;
	int sockfd;
	sigset_t set;

	/* blocking all signals in this thread */
	sigemptyset(&set);
	ret = pthread_sigmask(SIG_SETMASK, &set, NULL);
	if (ret) {
		printf("ERROR: pthread_sigmask: %s.\n", strerror(errno));
	}
	//printf("pthread_sigmask set successfully in thread.\n");
	
	sockfd = create_netlink_sockfd();
	if (sockfd < 0){
		printf("ERROR: Netlink socket creation error: %s.\n", strerror(errno));
		return (void *) -errno;
	}
	
	ret = send_netlink_message(sockfd, *pid);
	if (ret < 0) {
		printf("ERROR: Sending initial message to kernel on netlink socket failed: %s.\n", strerror(errno));
		return (void *) -errno;
	}
	
	receive_netlink_message(sockfd, *pid);
	
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
	
	sent_bytes = sendmsg(sockfd, &msg, 0);
	if (sent_bytes < 0) {
		printf("ERROR: Sending message to kernel failed: %s.\n", strerror(errno));
		ret = -1;
		goto out;
	}

out:
	return ret;
}

int receive_netlink_message(int sockfd, int pid)
{
	int ret = 0;
	ssize_t recv_bytes;
	fd_set rset;
	int s_ret;
	
	for (;;)
	{
		FD_ZERO(&rset);
		FD_SET(sockfd, &rset);
		
		//memset(&msg, 0, sizeof(msg));
		s_ret = select(FD_SETSIZE, &rset, NULL, NULL, NULL);
		if (s_ret < 0) {
			printf("ERROR: Select: %s\n", strerror(errno));
			ret = -errno;
			goto out;
		}
		if (FD_ISSET(sockfd, &rset)) {
				recv_bytes = recvmsg(sockfd, &msg, 0);
				//printf("Recv_bytes: %zd\n", recv_bytes);
				if (recv_bytes == 0)  { /* conn closed on other end */
					ret = 0;
					printf("ERROR: NETLINK_RECEIVE: %s\n", strerror(errno));
					continue;
					//goto out;
				}
				else if (ret < 0) {
					ret = -errno;
				}
				else {
					ret = 1; 
					job_ret = *((struct JobReturn *) NLMSG_DATA(nlh));
					/* Sending SIGUSR1 to main thread */
					kill(pid, SIGUSR1);
				}
		}
	}
out:
	return ret;
}
