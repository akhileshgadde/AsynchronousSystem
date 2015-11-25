obj-m += sys_submitjob.o

INC=/lib/modules/$(shell uname -r)/build/arch/x86/include

all: startjobs submit_job

startjobs: job.o startjobs.o
		gcc job.o startjobs.o -o startjobs

job.o: job.c job.h common.h
	gcc -Wall -Werror -I$(INC)/generated/uapi -I$(INC)/uapi -c job.c -o job.o

startjobs.o: startjobs.c job.h common.h
	gcc -Wall -Werror -I$(INC)/generated/uapi -I$(INC)/uapi -c startjobs.c -o startjobs.o
 
submit_job:
	    make -Wall -Werror -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f *.o
	rm -f startjobs
