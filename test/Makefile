obj-m += sys_submitjob.o

#sys_submitjob-objs := sys_netlink.o

INC=/lib/modules/$(shell uname -r)/build/arch/x86/include

all: startjobs submit_job

startjobs: job.o startjobs.o usr_netlink.o usr_signal.o 
	gcc -lpthread usr_signal.o job.o usr_netlink.o startjobs.o -o startjobs

job.o: job.c job.h common.h
	gcc -Wall -Werror -c job.c -o job.o

startjobs.o: startjobs.c job.h common.h
	gcc -Wall -Werror -I$(INC)/generated/uapi -I$(INC)/uapi -c startjobs.c -o startjobs.o

usr_netlink.o: usr_netlink.c usr_netlink.h common.h 
	gcc -Wall -Werror -c usr_netlink.c -o usr_netlink.o

usr_signal.o: usr_signal.c usr_signal.h
	gcc -Wall -Werror -c usr_signal.c -o usr_signal.o

submit_job:
	make -Wall -Werror -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f *.o
	rm -f startjobs
