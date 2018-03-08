#include <linux/module.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>

#define NETLINK_USER 31

struct sock *nl_sk = NULL;
//static struct netlink_kernel_cfg cfg = {0};

void netlink_recv_msg(struct sk_buff *skb);
