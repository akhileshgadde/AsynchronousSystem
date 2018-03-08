
#define NETLINK_USER 31
#define MAX_PAYLOAD 1024
#define SA struct sockaddr

int send_netlink_message(int sockfd, int pid);
int receive_netlink_message(int sockfd, int pid);
int create_netlink_sockfd();
void *netlink_process(void *args);
void set_sigusr1_signal();
