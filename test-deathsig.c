#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/procctl.h>
#include <sys/ptrace.h>
#include <sys/signal.h>
#include <sys/types.h>

static volatile sig_atomic_t got_signal = 0;

static void
my_signal_handler(int signum)
{
	if (signum == SIGINFO)
		got_signal = 1;
}

/*
 * Test that parent signals child on exit.
 */
static int
test1(void)
{
	int signum;
        int rc;
       
	rc = fork();
	assert(rc >= 0);
        if (rc == 0) {
		/* child */

		/* request a signal on parent death and register a handler */
		signum = SIGINFO;
		signal(signum, my_signal_handler);
		rc = procctl(P_PID, 0, PROC_SET_PDEATHSIG, &signum);
		assert(rc == 0);

		/* check that we can read the signal number back */
		signum = 0xdeadbeef;
		rc = procctl(P_PID, 0, PROC_GET_PDEATHSIG, &signum);
		assert(rc == 0);
		assert(signum == SIGINFO);

		/* wait for parent to die and signal us... */
                sleep(4);

		assert(got_signal == 1);

                printf("test passed\n");
        } else {
		/* parent */

		/* wait for a short time to give the child time to request a signal */
                sleep(2);

		/* exit, should signal child */
        }
        
        return 0;
}

/*
 * Test that child does not inherit parent's p_pdeathsig.
 */
static int
test2(void)
{
	int signum;
        int rc;

	/* request a signal on parent death in the parent */
	signum = SIGINFO;
	rc = procctl(P_PID, 0, PROC_SET_PDEATHSIG, &signum);

	rc = fork();
	assert(rc >= 0);
        if (rc == 0) {
		/* child */

		/* check that we didn't inherit the setting */
		signum = 0xdeadbeef;
		rc = procctl(P_PID, 0, PROC_GET_PDEATHSIG, &signum);
		assert(signum == 0);
		printf("test passed\n");
        }
        
        return 0;
}

/*
 * Test that various bad arguments are rejected.
 */
static int
test3(void)
{
	int signum;
        int rc;

	/* bad signal */
	signum = 8888;
	rc = procctl(P_PID, 0, PROC_SET_PDEATHSIG, &signum);
	assert(rc == -1);
	assert(errno == EINVAL);

	/* bad id type */
	signum = SIGINFO;
	rc = procctl(8888, 0, PROC_SET_PDEATHSIG, &signum);
	assert(rc == -1);
	assert(errno == EINVAL);

	/* bad id */
	signum = SIGINFO;
	rc = procctl(P_PID, 8888, PROC_SET_PDEATHSIG, &signum);
	assert(rc == -1);
	assert(errno == EINVAL);

	/* null pointer */
	signum = SIGINFO;
	rc = procctl(P_PID, 0, PROC_SET_PDEATHSIG, NULL);
	assert(rc == -1);
	assert(errno == EFAULT);

	/* good */
	signum = SIGINFO;
	rc = procctl(P_PID, 0, PROC_SET_PDEATHSIG, &signum);
	assert(rc == 0);

	printf("test passed\n");

	return 0;
}

/*
 * Test that exec'd process keeps the setting.  Test 4 execs test 1004.
 */
static int
test4(const char *path)
{
	int signum;
        int rc;
       
	/* request a signal on parent death and register a handler */
	signum = SIGINFO;
	rc = procctl(P_PID, 0, PROC_SET_PDEATHSIG, &signum);
	assert(rc == 0);

	/* execute test 1004, which will assert that it still has the setting */
	rc = execl(path, path, "1004", NULL);
	assert(rc == 0);
        
        return 0;
}

/*
 * Helper for test4(); will be executed to check that the setting was retained.
 */
static int
test1004(void)
{
	int signum;
	int rc;

	/* check that we can read the signal number back */
	signum = 0xdeadbeef;
	rc = procctl(P_PID, 0, PROC_GET_PDEATHSIG, &signum);
	assert(rc == 0);
	assert(signum == SIGINFO);
	printf("test passed\n");

	return 0;
}

/*
 * Put the child under trace (so it's reparented), and then see if it still gets
 * a signal when its real parent exits.
 */
static int
test5(void)
{
	int signum;
        int rc;
       
	rc = fork();
	assert(rc >= 0);
        if (rc == 0) {
		/* child */

		time_t wait_until = time(NULL) + 4;

		/* request a signal on parent death and register a handler */
		signum = SIGINFO;
		signal(signum, my_signal_handler);
		rc = procctl(P_PID, 0, PROC_SET_PDEATHSIG, &signum);
		assert(rc == 0);

		/* wait for parent to die and signal us... */
		while (time(NULL) < wait_until)
			sleep(1);

		assert(got_signal == 1);

		printf("test passed\n");
        } else {
		/* parent */
		pid_t pid_under_trace = rc;

		/* fork another process that will ptrace the child */
		rc = fork();
		assert(rc >= 0);
		if (rc == 0) {
			int status;

			/* tracer process */
			sleep(1);
			rc = ptrace(PT_ATTACH, pid_under_trace, 0, 0);
			assert(rc == 0);

			waitpid(pid_under_trace, &status, 0);
			assert(rc == 0);
			assert(WIFSTOPPED(status));
			assert(WSTOPSIG(status) == SIGSTOP);

			rc = ptrace(PT_CONTINUE, pid_under_trace, (caddr_t) 1, 0);
			assert(rc == 0);

			waitpid(pid_under_trace, &status, 0);
			assert(rc == 0);
			assert(WIFSTOPPED(status));
			assert(WSTOPSIG(status) == SIGINFO);

			rc = ptrace(PT_CONTINUE, pid_under_trace, (caddr_t) 1, WSTOPSIG(status));
			assert(rc == 0);

			sleep(4);

			ptrace(PT_DETACH, pid_under_trace, 0, 0);
			return 0;
		}

		/* wait for a short time to give the child time to request a signal */
                sleep(2);

		/* exit, should signal child even though child is being traced */
        }
        
        return 0;
}

int
main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s test, where test is 1, 2, 3, 4, 5\n", argv[0]);
		return 1;
	}
	switch (atoi(argv[1])) {
	case 1:
		return test1();
	case 2:
		return test2();
	case 3:
		return test3();
	case 4:
		return test4(argv[0]);
	case 5:
		return test5();
	case 1004:
		return test1004();
	default:
		fprintf(stderr, "unknown test\n");
		return 1;
	}
}
