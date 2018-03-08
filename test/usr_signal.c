#include "usr_signal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

void usr_sig_process(int signal)
{
    if (signal == SIGUSR1)
        printf("received signal SIGUSR1 and i/p: %s\n", job_ret.input_file);
    else
        printf("Received diff signal.\n");
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
