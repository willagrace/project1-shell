/*
 * test_expand.c - unit tests for Part 2 (environment variables) and Part 3 (tilde).
 *
 * Interface under test (include/expand.h):
 *     expand_env_vars()  "$NAME" tokens -> getenv("NAME"); unset -> "" (our choice)
 *     expand_tilde()     "~" and "~/..." tokens -> $HOME + the rest
 *     expand_tokens()    both, tilde first (so a variable whose value is "~" stays "~")
 *
 * Besides the expected values, these checks catch the memory mistakes that are easy
 * to make here: writing a long value into the old, shorter token buffer, losing the
 * NULL terminator that execv needs, and modifying the environment itself.
 *
 * How: each test tokenizes a line with the real lexer, sets or unsets the variables
 * it needs with setenv/unsetenv, runs the expansion, and compares the tokens.
 *
 * Run with: make test
 */

#define _POSIX_C_SOURCE 200809L /* POSIX: setenv, unsetenv, strdup on Linux/glibc */

#include "expand.h"
#include "test.h"
#include <stdlib.h>
#include <string.h>

/* Tokenize a line the same way the shell does. */
static tokenlist *tokenize(const char *line)
{
	char *buf = strdup(line);
	tokenlist *tokens = get_tokens(buf);
	free(buf);
	return tokens;
}

/* Part 2 spec: tokens starting with '$' are replaced by the variable's value,
 * for any command, but only when the whole token is the variable. */
static void test_env_vars(void)
{
	setenv("TEST_VAR", "hello", 1);
	unsetenv("TEST_UNSET_VAR");

	tokenlist *t = tokenize("echo $TEST_VAR");
	expand_env_vars(t);
	CHECK(t->size == 2, "size should be 2, got %zu", t->size);
	CHECK_STR(t->items[0], "echo");
	CHECK_STR(t->items[1], "hello");
	CHECK(t->items[t->size] == NULL, "list must stay NULL-terminated for execv");
	free_tokens(t);

	/* variable as the command itself */
	t = tokenize("$TEST_VAR");
	expand_env_vars(t);
	CHECK_STR(t->items[0], "hello");
	free_tokens(t);

	/* several variables in one command */
	t = tokenize("$TEST_VAR mid $TEST_VAR");
	expand_env_vars(t);
	CHECK_STR(t->items[0], "hello");
	CHECK_STR(t->items[1], "mid");
	CHECK_STR(t->items[2], "hello");
	free_tokens(t);

	/* value much longer than the "$NAME" token (catches writing into the old buffer) */
	setenv("TEST_VAR", "a-value-that-is-much-longer-than-the-token-itself", 1);
	t = tokenize("echo $TEST_VAR");
	expand_env_vars(t);
	CHECK_STR(t->items[1], "a-value-that-is-much-longer-than-the-token-itself");
	free_tokens(t);
	setenv("TEST_VAR", "hello", 1);

	/* unset variable becomes an empty string (our chosen behavior) */
	t = tokenize("echo $TEST_UNSET_VAR hi");
	expand_env_vars(t);
	CHECK(t->size == 3, "size should be 3, got %zu", t->size);
	CHECK_STR(t->items[1], "");
	CHECK_STR(t->items[2], "hi");
	free_tokens(t);

	/* a lone "$" is left alone */
	t = tokenize("echo $");
	expand_env_vars(t);
	CHECK_STR(t->items[1], "$");
	free_tokens(t);

	/* only whole tokens are expanded */
	t = tokenize("echo a$TEST_VAR");
	expand_env_vars(t);
	CHECK_STR(t->items[1], "a$TEST_VAR");
	free_tokens(t);

	/* the environment itself must not be changed */
	CHECK_STR(getenv("TEST_VAR"), "hello");
}

/* Part 3 spec: expand "~" only when it is standalone or begins the token as "~/". */
static void test_tilde(void)
{
	setenv("HOME", "/home/tester", 1);

	tokenlist *t = tokenize("ls ~");
	expand_tilde(t);
	CHECK_STR(t->items[0], "ls");
	CHECK_STR(t->items[1], "/home/tester");
	free_tokens(t);

	t = tokenize("ls ~/dir1");
	expand_tilde(t);
	CHECK_STR(t->items[1], "/home/tester/dir1");
	free_tokens(t);

	/* tilde as the first token */
	t = tokenize("~/bin/prog arg");
	expand_tilde(t);
	CHECK_STR(t->items[0], "/home/tester/bin/prog");
	CHECK_STR(t->items[1], "arg");
	free_tokens(t);

	/* several tildes in one command */
	t = tokenize("cp ~/a ~/b");
	expand_tilde(t);
	CHECK_STR(t->items[1], "/home/tester/a");
	CHECK_STR(t->items[2], "/home/tester/b");
	free_tokens(t);

	/* forms that must NOT be expanded */
	t = tokenize("ls ~x a~/b ~~ /~");
	expand_tilde(t);
	CHECK_STR(t->items[1], "~x");
	CHECK_STR(t->items[2], "a~/b");
	CHECK_STR(t->items[3], "~~");
	CHECK_STR(t->items[4], "/~");
	free_tokens(t);

	/* HOME unset: leave tokens unchanged */
	unsetenv("HOME");
	t = tokenize("ls ~ ~/x");
	expand_tilde(t);
	CHECK_STR(t->items[1], "~");
	CHECK_STR(t->items[2], "~/x");
	free_tokens(t);
	setenv("HOME", "/home/tester", 1);
}

/* The combined entry point main.c calls: both expansions, in the right order. */
static void test_expand_tokens(void)
{
	setenv("HOME", "/home/tester", 1);
	unsetenv("TEST_UNSET_VAR");

	/* both expansions in one command */
	setenv("TEST_VAR", "hello", 1);
	tokenlist *t = tokenize("echo $TEST_VAR ~/x");
	expand_tokens(t);
	CHECK_STR(t->items[1], "hello");
	CHECK_STR(t->items[2], "/home/tester/x");
	free_tokens(t);

	/* a variable whose value is "~" is not tilde-expanded again (matches bash) */
	setenv("TEST_VAR", "~", 1);
	t = tokenize("echo $TEST_VAR");
	expand_tokens(t);
	CHECK_STR(t->items[1], "~");
	free_tokens(t);

	/* unset variable next to a tilde */
	t = tokenize("echo $TEST_UNSET_VAR ~");
	expand_tokens(t);
	CHECK_STR(t->items[1], "");
	CHECK_STR(t->items[2], "/home/tester");
	free_tokens(t);

	/* empty input must not crash */
	t = tokenize("");
	expand_tokens(t);
	CHECK(t->size == 0, "empty input should give 0 tokens, got %zu", t->size);
	free_tokens(t);
}

int main(void)
{
	test_env_vars();
	test_tilde();
	test_expand_tokens();
	return TEST_SUMMARY("test_expand");
}
