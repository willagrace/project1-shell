/*
 * test_jobs.c - unit tests for Part 8 (background processing: job tracking).
 *
 * What is tested (src/jobs.c):
 *   - is_background():   recognizes a trailing "&" token
 *   - add_background_job(): prints "[N] PID" and hands out job numbers 1, 2, 3, ...
 *   - print_jobs():      the "jobs" listing "[N]+ PID CMD", or a message when empty
 *   - check_background_jobs(): reports "[N]+ done CMD" once, only after a job exits
 *   - wait_for_background_jobs(): blocks until every job exits (used by "exit")
 *
 * How: real child processes stand in for background commands. Each child blocks
 * reading a pipe, so the test decides exactly when it finishes by closing the
 * write end. Nothing is timing-based, so the results are the same on every run.
 *
 * Run with: make test
 */

#define _XOPEN_SOURCE 700 /* POSIX: pipe, fork, waitid, siginfo_t on Linux/glibc */

#include "jobs.h"
#include "test.h"
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* A child process the test controls: it exits once release_fd is closed. */
typedef struct {
	pid_t pid;
	int release_fd;
} child_t;

/* Write ends of every pipe still held by the test. A new child must close these,
 * or an earlier child would never see end-of-file and would never exit. */
static int open_release_fds[16];
static int open_release_count = 0;

/* Fork a child that exits as soon as release_child() is called. */
static child_t start_child(void)
{
	child_t child;
	int fds[2];
	char byte;

	if (pipe(fds) != 0) {
		perror("pipe");
		exit(1);
	}
	child.pid = fork();
	if (child.pid == 0) {
		/* child: drop every write end, then block until the pipe reaches EOF */
		close(fds[1]);
		for (int i = 0; i < open_release_count; i++)
			close(open_release_fds[i]);
		while (read(fds[0], &byte, 1) > 0)
			;
		_exit(0);
	}
	/* parent: keep only the write end; closing it later releases the child */
	close(fds[0]);
	child.release_fd = fds[1];
	open_release_fds[open_release_count++] = fds[1];
	return child;
}

/* Let the child exit, and wait until it has exited WITHOUT reaping it (WNOWAIT),
 * so the code under test is still the one that collects it. */
static void release_child(child_t child)
{
	siginfo_t info;
	close(child.release_fd);
	waitid(P_PID, child.pid, &info, WEXITED | WNOWAIT);
}

/* Run check_background_jobs() and return what it printed (caller frees). */
static char *run_check(void)
{
	capture_t c = capture_begin(stdout);
	check_background_jobs();
	return capture_end(c);
}

/* Run print_jobs() and return what it printed (caller frees). */
static char *run_print_jobs(void)
{
	capture_t c = capture_begin(stdout);
	print_jobs();
	return capture_end(c);
}

/* Only a separate, final "&" token means "run in the background". */
static void test_is_background(void)
{
	tokenlist *t = get_tokens("sleep 5 &");
	CHECK(is_background(t), "\"sleep 5 &\" should be background");
	free_tokens(t);

	t = get_tokens("sleep 5");
	CHECK(!is_background(t), "\"sleep 5\" should not be background");
	free_tokens(t);

	/* spec: "cmd1 | cmd2 &" runs the whole pipeline in the background */
	t = get_tokens("echo a | wc &");
	CHECK(is_background(t), "a pipeline ending in & should be background");
	free_tokens(t);

	t = get_tokens("echo a&b");
	CHECK(!is_background(t), "& inside a token is not the background operator");
	free_tokens(t);

	t = new_tokenlist();
	CHECK(!is_background(t), "empty command should not be background");
	free_tokens(t);
}

/* Follows two jobs from start to finish, checking every message the spec requires. */
static void test_job_lifecycle(void)
{
	char expected[128];
	char *out;

	jobs_init();

	/* spec: "If there are no active background processes, say so." */
	out = run_print_jobs();
	CHECK(strlen(out) > 0 && strstr(out, "[") == NULL,
		"with no jobs, print_jobs should say so, got \"%s\"", out);
	free(out);

	/* spec: "Upon execution start, print [Job number] [cmd's PID]" */
	child_t a = start_child();
	capture_t c = capture_begin(stdout);
	int num_a = add_background_job(a.pid, "sleep 10 &");
	out = capture_end(c);
	snprintf(expected, sizeof(expected), "[1] %d\n", (int)a.pid);
	CHECK(num_a == 1, "first job number should be 1, got %d", num_a);
	CHECK_STR(out, expected);
	free(out);

	child_t b = start_child();
	c = capture_begin(stdout);
	int num_b = add_background_job(b.pid, "ls | wc &");
	out = capture_end(c);
	snprintf(expected, sizeof(expected), "[2] %d\n", (int)b.pid);
	CHECK(num_b == 2, "second job number should be 2, got %d", num_b);
	CHECK_STR(out, expected);
	free(out);

	/* spec (jobs): "[Job number]+ [CMD's PID] [CMD's command line]" for each job */
	out = run_print_jobs();
	snprintf(expected, sizeof(expected), "[1]+ %d sleep 10 &", (int)a.pid);
	CHECK(strstr(out, expected) != NULL,
		"jobs should list \"%s\", got \"%s\"", expected, out);
	snprintf(expected, sizeof(expected), "[2]+ %d ls | wc &", (int)b.pid);
	CHECK(strstr(out, expected) != NULL,
		"jobs should list \"%s\", got \"%s\"", expected, out);
	free(out);

	/* nothing is reported while jobs are still running */
	out = run_check();
	CHECK_STR(out, "");
	free(out);

	/* spec: "Upon completion, print [Job number] + done [cmd's command line]" */
	release_child(a);
	out = run_check();
	CHECK(strstr(out, "[1]") != NULL && strstr(out, "done") != NULL &&
		strstr(out, "sleep 10 &") != NULL,
		"job 1 should be reported done with its command line, got \"%s\"", out);
	CHECK(strstr(out, "[2]") == NULL, "job 2 is still running, got \"%s\"", out);
	free(out);

	/* ...and only once: the next check has nothing new to report */
	out = run_check();
	CHECK_STR(out, "");
	free(out);

	/* a finished job leaves the jobs list; a running one stays */
	out = run_print_jobs();
	CHECK(strstr(out, "sleep 10") == NULL,
		"finished job should leave jobs list, got \"%s\"", out);
	CHECK(strstr(out, "ls | wc") != NULL,
		"running job should stay in jobs list, got \"%s\"", out);
	free(out);

	release_child(b);
	out = run_check();
	CHECK(strstr(out, "[2]") != NULL && strstr(out, "done") != NULL,
		"job 2 should be reported done, got \"%s\"", out);
	free(out);

	/* spec: "Job numbers will not be reused" - slot 1 is free again, but the number is 3 */
	child_t d = start_child();
	c = capture_begin(stdout);
	int num_d = add_background_job(d.pid, "sleep 1 &");
	out = capture_end(c);
	CHECK(num_d == 3, "job numbers must not be reused: expected 3, got %d", num_d);
	free(out);

	/* spec (exit): "If any background processes are still running, you must wait" */
	close(d.release_fd);
	c = capture_begin(stdout);
	wait_for_background_jobs();
	out = capture_end(c);
	free(out);
	/* waitpid returns -1 when there is no such child left, i.e. it was already reaped */
	CHECK(waitpid(d.pid, NULL, WNOHANG) == -1, "job should already have been reaped");

	out = run_print_jobs();
	CHECK(strstr(out, "[") == NULL, "no jobs should remain, got \"%s\"", out);
	free(out);
}

int main(void)
{
	test_is_background();
	test_job_lifecycle();
	return TEST_SUMMARY("test_jobs");
}
