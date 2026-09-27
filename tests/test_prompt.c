/*
 * test_prompt.c - unit tests for Part 1 (prompt).
 *
 * The spec requires the prompt "USER@MACHINE:PWD> " with the absolute working
 * directory, printed without a newline so the user types on the same line.
 *
 * How: print_prompt() is called with stdout captured, and the result is compared
 * to a prompt built independently from $USER, gethostname() and getcwd().
 *
 * Run with: make test
 */

#define _XOPEN_SOURCE 700 /* POSIX: gethostname, setenv on Linux/glibc */

#include "prompt.h"
#include "test.h"
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>

/* Build the prompt we expect for the given user and the current host and directory. */
static void expected_prompt(char *buf, size_t size, const char *user)
{
	char host[256];
	char cwd[PATH_MAX];
	if (gethostname(host, sizeof(host)) != 0)
		host[0] = '\0';
	if (getcwd(cwd, sizeof(cwd)) == NULL)
		cwd[0] = '\0';
	snprintf(buf, size, "%s@%s:%s> ", user, host, cwd);
}

int main(void)
{
	char expected[2 * PATH_MAX];

	/* the basic format */
	setenv("USER", "tester", 1);
	capture_t c = capture_begin(stdout);
	print_prompt();
	char *out = capture_end(c);
	expected_prompt(expected, sizeof(expected), "tester");
	CHECK_STR(out, expected);
	CHECK(strchr(out, '\n') == NULL,
		"prompt must not end the line (user types on the same line)");
	free(out);

	/* the prompt follows the working directory (e.g. after cd) */
	char old_cwd[PATH_MAX];
	if (getcwd(old_cwd, sizeof(old_cwd)) != NULL && chdir("/") == 0) {
		c = capture_begin(stdout);
		print_prompt();
		out = capture_end(c);
		expected_prompt(expected, sizeof(expected), "tester");
		CHECK_STR(out, expected);
		free(out);
		if (chdir(old_cwd) != 0)
			perror("chdir");
	}

	/* the prompt follows $USER */
	setenv("USER", "someone_else", 1);
	c = capture_begin(stdout);
	print_prompt();
	out = capture_end(c);
	expected_prompt(expected, sizeof(expected), "someone_else");
	CHECK_STR(out, expected);
	free(out);

	return TEST_SUMMARY("test_prompt");
}
