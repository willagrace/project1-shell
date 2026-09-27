#pragma once

/*
 * test.h - minimal test helpers shared by the unit tests in tests/.
 *
 *   CHECK(cond, fmt, ...)       one check; on failure prints file:line and the message
 *   CHECK_STR(actual, expected) one check comparing two strings (either may be NULL)
 *   TEST_SUMMARY(name)          prints "name: passed/total" and gives the exit status
 *   capture_begin/capture_end   collect what a function prints to stdout or stderr
 *
 * Each test_*.c file is its own program: make test compiles it together with every
 * object file except main.o (the shell's main), runs it, and reports its summary.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int tests_run = 0;
static int tests_failed = 0;

/* Check that a condition holds; the remaining arguments are a printf-style message. */
#define CHECK(cond, ...) do { \
	tests_run++; \
	if (!(cond)) { \
		tests_failed++; \
		printf("  FAIL %s:%d: ", __FILE__, __LINE__); \
		printf(__VA_ARGS__); \
		printf("\n"); \
	} \
} while (0)

/* Check that two strings are equal (either may be NULL). */
#define CHECK_STR(actual, expected) do { \
	const char *a_ = (actual); \
	const char *e_ = (expected); \
	tests_run++; \
	if ((a_ == NULL) != (e_ == NULL) || (a_ != NULL && strcmp(a_, e_) != 0)) { \
		tests_failed++; \
		printf("  FAIL %s:%d: %s\n    expected: %s\n    actual:   %s\n", \
			__FILE__, __LINE__, #actual, \
			e_ ? e_ : "(null)", a_ ? a_ : "(null)"); \
	} \
} while (0)

/* Print a one-line summary; evaluates to the process exit status (0 = all passed). */
#define TEST_SUMMARY(name) \
	(printf("%s: %d/%d checks passed\n", name, tests_run - tests_failed, tests_run), \
	 tests_failed == 0 ? 0 : 1)

/* Capture everything written to a stream (stdout or stderr) between
 * capture_begin() and capture_end(). capture_end() returns a malloc'd string. */
typedef struct {
	FILE *stream;
	int saved_fd;
	FILE *file;
} capture_t;

static inline capture_t capture_begin(FILE *stream)
{
	capture_t c;
	fflush(stream);
	c.stream = stream;
	c.file = tmpfile();
	c.saved_fd = dup(fileno(stream));
	dup2(fileno(c.file), fileno(stream));
	return c;
}

static inline char *capture_end(capture_t c)
{
	fflush(c.stream);
	dup2(c.saved_fd, fileno(c.stream));
	close(c.saved_fd);

	long len = ftell(c.file);
	char *text = (char *)malloc(len > 0 ? len + 1 : 1);
	rewind(c.file);
	size_t n = len > 0 ? fread(text, 1, len, c.file) : 0;
	text[n] = '\0';
	fclose(c.file);
	return text;
}
