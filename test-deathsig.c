#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/procctl.h>
#include <sys/signal.h>

static void
my_signal_handler(int signum)
{
	printf("Got signal %d!\n", signum);
}

int main(int argc, char **argv)
{
        int rc = fork();

        if (rc < 0) {
                perror("fork");
                return 1;
        } else if (rc == 0) {
		int signum = SIGINFO;

		signal(signum, my_signal_handler);
		rc = procctl(P_PID, 0, 11, &signum);
                printf("hello from child, procctl ret = %d, errno = %d\n", rc, errno);
                sleep(4);
                printf("goodbye from child\n");
        } else {
                printf("hello from parent, child pid = %d\n", rc);
                sleep(2);
                printf("goodbye from parent\n");
        }
        
        return 0;
}
