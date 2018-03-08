#include "sys_netlink.h"

void netlink_recv_msg(struct sk_buff *skb)
{
	struct nlmsghdr *nlh;
	int pid;
	struct sk_buff *skb_out;
	int msg_size;
	char *msg = "Hello from kernel";
	int res;
	
	printk(KERN_INFO "Entering: %s\n", __FUNCTION__);
    msg_size = strlen(msg);
	
	nlh = (struct nlmsghdr *) skb->data;
	printk(KERN_INFO "Netlink received msg payload: %s\n", (char *)nlmsg_data(nlh));
	pid = nlh->nlmsg_pid;
	
	skb_out = nlmsg_new(msg_size, 0);
	if (!skb_out) {
        printk(KERN_ERR "Failed to allocate new skb\n");
        //res = -ENOMEM;
		//goto out;
		return;
    }
	nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);
	NETLINK_CB(skb_out).dst_group = 0; /* not in mcast group */
	strncpy(nlmsg_data(nlh), msg, msg_size);
	
	res = nlmsg_unicast(nl_sk, skb_out, pid);
    if (res < 0)
        printk(KERN_INFO "Error while sending bak to user\n");

	return;
}
