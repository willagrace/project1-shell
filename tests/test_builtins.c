/*
 * test_builtins.c - unit tests for Part 9 (internal commands: cd, jobs, exit).
 *
 * What is tested (src/builtins.c):
 *   - is_builtin():      only "cd", "jobs" and "exit" (as the command name) are built-ins
 *   - cd:                changes directory, goes to $HOME with no argument, and reports
 *                        the three errors the spec requires without changing directory
 *   - jobs:              says so when there are no background processes
 *   - exit + history:    returns SHELL_EXIT and prints the last three valid commands
 *
 * How: each built-in runs in this process (just like in the shell) with stdout and
 * stderr captured. cd tests run inside a scratch directory made with mkdtemp:
 *     <root>/dir/       a directory to cd into
 *     <root>/home/      used as $HOME
 *     <root>/file.txt   a regular file (cd into it must fail)
 *
 * Run with: make test
 */

#define _XOPEN_SOURCE 700 /* POSIX: mkdtemp, realpath, setenv, strdup on Linux/glibc */
#define _DARWIN_C_SOURCE  /* macOS hides mkdtemp without this; ignored on Linux */

#include "builtins.h"
#include "jobs.h"
#include "test.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char root[] = "/tmp/shell_builtin_test_XXXXXX";
static char real_root[PATH_MAX]; /* root with symlinks resolved, to compare with getcwd */

/* Tokenize a line the same way the shell does. */
static tokenlist *tokenize(const char *line)
{
	char *buf = strdup(line);
	tokenlist *tokens = get_tokens(buf);
	free(buf);
	return tokens;
}

/* Run one built-in; its stdout/stderr are stored in *out / *err (caller frees). */
static int run_builtin(const char *line, char **out, char **err)
{
	tokenlist *t = tokenize(line);
	capture_t co = capture_begin(stdout);
	capture_t ce = capture_begin(stderr);
	int result = execute_builtin(t);
	*err = capture_end(ce);
	*out = capture_end(co);
	free_tokens(t);
	return result;
}

/* The current working directory (static buffer, overwritten on each call). */
static const char *cwd(void)
{
	static char buf[PATH_MAX];
	if (getcwd(buf, sizeof(buf)) == NULL)
		buf[0] = '\0';
	return buf;
}

/* Built-ins are matched on the command name only, and must match exactly. */
static void test_is_builtin(void)
{
	const char *yes[] = { "cd", "cd /tmp", "jobs", "exit" };
	const char *no[] = { "ls", "echo cd", "cdx", "exits", "/bin/cd" };

	for (size_t i = 0; i < sizeof(yes) / sizeof(yes[0]); i++) {
		tokenlist *t = tokenize(yes[i]);
		CHECK(is_builtin(t), "\"%s\" should be a built-in", yes[i]);
		free_tokens(t);
	}
	for (size_t i = 0; i < sizeof(no) / sizeof(no[0]); i++) {
		tokenlist *t = tokenize(no[i]);
		CHECK(!is_builtin(t), "\"%s\" should not be a built-in", no[i]);
		free_tokens(t);
	}
}

static void test_cd(void)
{
	char path[2 * PATH_MAX];
	char *out;
	char *err;

	/* spec: "cd PATH - Changes the current working directory." */
	snprintf(path, sizeof(path), "cd %s/dir", real_root);
	CHECK(run_builtin(path, &out, &err) == 0, "cd into a directory should succeed");
	snprintf(path, sizeof(path), "%s/dir", real_root);
	CHECK_STR(cwd(), path);
	CHECK_STR(err, "");
	free(out);
	free(err);

	/* spec: "If no arguments are supplied, change the current working directory to $HOME" */
	snprintf(path, sizeof(path), "%s/home", real_root);
	setenv("HOME", path, 1);
	CHECK(run_builtin("cd", &out, &err) == 0, "plain cd should succeed");
	CHECK_STR(cwd(), path);
	free(out);
	free(err);

	/* spec: signal an error for more than one argument, a target that is not a
	 * directory, and a target that does not exist. Each must fail, print a message,
	 * and leave the working directory unchanged. */
	const char *bad[] = { "cd a b", "cd does_not_exist", "cd file.txt" };
	const char *why[] = { "too many arguments", "does not exist", "not a directory" };
	if (chdir(real_root) != 0)
		perror("chdir");
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
		int result = run_builtin(bad[i], &out, &err);
		CHECK(result != 0 && result != SHELL_EXIT,
			"\"%s\" (%s) should fail, returned %d", bad[i], why[i], result);
		CHECK(strlen(err) > 0, "\"%s\" (%s) should print an error", bad[i], why[i]);
		CHECK_STR(cwd(), real_root);
		free(out);
		free(err);
	}
}

/* spec (jobs): "If there are no active background processes, say so." */
static void test_jobs_builtin(void)
{
	char *out;
	char *err;

	jobs_init();
	CHECK(run_builtin("jobs", &out, &err) == 0, "jobs should succeed");
	CHECK(strlen(out) > 0 && strstr(out, "[") == NULL,
		"with no background jobs, jobs should say so, got \"%s\"", out);
	free(out);
	free(err);
}

/* History is global state in builtins.c, so these checks run in a fixed order,
 * each one adding to the history left by the previous one. */
static void test_exit_and_history(void)
{
	char *out;
	char *err;

	/* spec: "If there are no valid commands, say so." */
	CHECK(run_builtin("exit", &out, &err) == SHELL_EXIT, "exit should return SHELL_EXIT");
	CHECK(strlen(out) > 0, "exit with no history should say there were no valid commands");
	free(out);
	free(err);

	/* spec: "If there were less than three valid commands, print the last valid one." */
	add_to_history("ls -al");
	run_builtin("exit", &out, &err);
	CHECK(strstr(out, "ls -al") != NULL,
		"exit should show the last valid command, got \"%s\"", out);
	free(out);
	free(err);

	/* spec: "Display the last three valid commands." - the oldest one drops off */
	add_to_history("echo two");
	add_to_history("echo three");
	add_to_history("echo four");
	run_builtin("exit", &out, &err);
	char *p2 = strstr(out, "echo two");
	char *p3 = strstr(out, "echo three");
	char *p4 = strstr(out, "echo four");
	CHECK(strstr(out, "ls -al") == NULL, "only the last three commands, got \"%s\"", out);
	CHECK(p2 != NULL && p3 != NULL && p4 != NULL,
		"last three commands missing, got \"%s\"", out);
	CHECK(p2 == NULL || p3 == NULL || p4 == NULL || (p2 < p3 && p3 < p4),
		"commands should be in order, got \"%s\"", out);
	free(out);
	free(err);

	/* spec: "You can assume that each command is less than 200 characters long." */
	char long_cmd[200];
	memset(long_cmd, 'x', sizeof(long_cmd) - 1);
	long_cmd[sizeof(long_cmd) - 1] = '\0';
	add_to_history(long_cmd);
	run_builtin("exit", &out, &err);
	CHECK(strstr(out, long_cmd) != NULL, "a 199-character command should be kept intact");
	free(out);
	free(err);
}

int main(void)
{
	char path[2 * PATH_MAX];

	/* build the scratch directory tree described at the top of this file */
	if (mkdtemp(root) == NULL || realpath(root, real_root) == NULL) {
		perror("mkdtemp");
		return 1;
	}
	snprintf(path, sizeof(path), "%s/dir", real_root);
	mkdir(path, 0755);
	snprintf(path, sizeof(path), "%s/home", real_root);
	mkdir(path, 0755);
	snprintf(path, sizeof(path), "%s/file.txt", real_root);
	FILE *f = fopen(path, "w");
	if (f != NULL)
		fclose(f);

	char old_cwd[PATH_MAX];
	if (getcwd(old_cwd, sizeof(old_cwd)) == NULL)
		old_cwd[0] = '\0';

	test_is_builtin();
	test_cd();
	test_jobs_builtin();
	test_exit_and_history();

	/* go back to where we started and remove the scratch tree (path = file.txt) */
	if (chdir(old_cwd) != 0)
		perror("chdir");
	remove(path);
	snprintf(path, sizeof(path), "%s/dir", real_root);
	rmdir(path);
	snprintf(path, sizeof(path), "%s/home", real_root);
	rmdir(path);
	rmdir(real_root);

	return TEST_SUMMARY("test_builtins");
}
