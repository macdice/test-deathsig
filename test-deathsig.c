#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/procctl.h>
#include <sys/signal.h>

static void
my_signal_handler(int signum)
{
	printf("got signal %d\n", signum);
}

int main(int argc, char **argv)
{
	int signum;
        int rc = fork();

        if (rc < 0) {
                perror("fork");
                return 1;
        } else if (rc == 0) {
		/* child */

		/* request a signal on parent death and register a handler */
		signum = SIGINFO;
		signal(signum, my_signal_handler);
		rc = procctl(P_PID, 0, PROC_SET_PDEATHSIG, &signum);
                printf("procctl(P_PID, 0, PROC_SET_PDEATHSIG, &signum) returned = %d, errno = %d\n", rc, errno);

		/* check that we can read the signal number back */
		signum = 0xdeadbeef;
		rc = procctl(P_PID, 0, PROC_GET_PDEATHSIG, &signum);
                printf("procctl(P_PID, 0, PROC_GET_PDEATHSIG, &signum) returned = %d, errno = %d, signum = %d\n", rc, errno, signum);

		/* wait for parent to die and signal us... */
                sleep(4);

                printf("goodbye from child\n");
        } else {
		/* parent */

		/* wait for a short time to give the child time to request a signal */
                sleep(2);

		/* exit, should signal child */
        }
        
        return 0;
}
