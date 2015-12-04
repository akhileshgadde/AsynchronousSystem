/* Setting the SIGUSR handler for the main thread.
*  Signal handler to handle the received SIGUSR1 signal.
*  Prints the job processed status received from the kernel.
*/

#include "usr_signal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

void usr_sig_process(int signal)
{
    if (signal == SIGUSR1) {
		printf("---------------------------------------------\n");
		printf("Message from Kernel:\n");
		switch(job_ret.job_type) {
			case 0: 
				printf("Encryption ");
				break;
			case 1:
				printf("Decryption ");
		}
        printf("of file: \"%s\" ", job_ret.input_file);
		switch(job_ret.ret){
			case 0:
				printf("successful: ");
				break;
			default:
				printf("unsuccessful: ");
		}
		printf("%d\n", job_ret.ret);
		printf("---------------------------------------------\n");
	}
    else
        printf("Received incorrect signal.Ignoring..\n");
	return;
}

void set_sigusr1_signal()
{
	struct sigaction sigs;
	/* signals related stuff */
    memset(&sigs, 0, sizeof(struct sigaction));
    sigs.sa_handler = &usr_sig_process;
    sigs.sa_flags = 0;
    if (sigaction(SIGUSR1, &sigs, NULL) == -1)
	    perror("Unable to handle SIGUSR1");
}
