/*
 * test_path_search.c - unit tests for Part 4 ($PATH search).
 *
 * Interface under test (include/path_search.h):
 *     char *search_path(const char *cmd);
 * Returns a malloc'd path to the executable (caller frees), or NULL if not found.
 *
 * What is tested:
 *   - real commands (ls) are found; unknown ones return NULL ("command not found")
 *   - directories in $PATH are searched in order and the first match wins
 *   - a match must be a regular, executable file (not a directory, not rw- only)
 *   - "." in $PATH means the current directory; missing directories are skipped
 *   - commands containing '/' are returned as-is, without searching
 *   - the $PATH string itself is never modified (a classic strtok mistake)
 *   - $PATH unset and an empty command name do not crash
 *
 * How: setup() builds a small fake filesystem in /tmp (described above setup())
 * and each test points $PATH at parts of it, so results never depend on what
 * happens to be installed on the machine.
 *
 * Run with: make test
 */

#define _POSIX_C_SOURCE 200809L /* POSIX: mkdtemp, setenv, strdup on Linux/glibc */
#define _DARWIN_C_SOURCE /* macOS hides mkdtemp without this; ignored on Linux */

#include "path_search.h"
#include "test.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char root[] = "/tmp/shell_path_test_XXXXXX";
static char dir_a[PATH_MAX];
static char dir_b[PATH_MAX];

/* Create an empty file at dir/name with the given permission bits. */
static void make_file(const char *dir, const char *name, mode_t mode)
{
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/%s", dir, name);
	FILE *f = fopen(path, "w");
	if (f != NULL)
		fclose(f);
	chmod(path, mode);
}

/* Build a fake filesystem:
 *   dir_a/both      executable    (also in dir_b; dir_a comes first in PATH)
 *   dir_a/only_b    NOT executable (a real one exists in dir_b)
 *   dir_a/subdir/   a directory, not a program
 *   dir_b/both      executable
 *   dir_b/only_b    executable
 *   dir_b/local     executable    (used to test the "." PATH entry) */
static int setup(void)
{
	if (mkdtemp(root) == NULL) {
		perror("mkdtemp");
		return -1;
	}
	snprintf(dir_a, sizeof(dir_a), "%s/a", root);
	snprintf(dir_b, sizeof(dir_b), "%s/b", root);
	mkdir(dir_a, 0755);
	mkdir(dir_b, 0755);

	make_file(dir_a, "both", 0755);
	make_file(dir_a, "only_b", 0644);
	make_file(dir_b, "both", 0755);
	make_file(dir_b, "only_b", 0755);
	make_file(dir_b, "local", 0755);

	char subdir[PATH_MAX + 16]; /* dir_a + "/subdir" */
	snprintf(subdir, sizeof(subdir), "%s/subdir", dir_a);
	mkdir(subdir, 0755);
	return 0;
}

static void teardown(void)
{
	const char *files[] = { "a/both", "a/only_b", "b/both", "b/only_b", "b/local" };
	char path[PATH_MAX + 16];   /* room for dir_a + "/subdir" */
	for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
		snprintf(path, sizeof(path), "%s/%s", root, files[i]);
		unlink(path);
	}
	snprintf(path, sizeof(path), "%s/subdir", dir_a);
	rmdir(path);
	rmdir(dir_a);
	rmdir(dir_b);
	rmdir(root);
}

/* Set PATH to "first:second" (second may be NULL). */
static void set_path(const char *first, const char *second)
{
	char value[2 * PATH_MAX + 2];
	if (second == NULL)
		snprintf(value, sizeof(value), "%s", first);
	else
		snprintf(value, sizeof(value), "%s:%s", first, second);
	setenv("PATH", value, 1);
}

/* Build the expected "dir/name" string for comparisons. */
static const char *join(const char *dir, const char *name)
{
	static char buf[PATH_MAX];
	snprintf(buf, sizeof(buf), "%s/%s", dir, name);
	return buf;
}

/* Sanity check against the real system: ls exists everywhere, the other does not. */
static void test_real_commands(void)
{
	setenv("PATH", "/usr/bin:/bin", 1);

	char *p = search_path("ls");
	CHECK(p != NULL, "ls should be found in /usr/bin:/bin");
	if (p != NULL) {
		size_t n = strlen(p);
		CHECK(n >= 3 && strcmp(p + n - 3, "/ls") == 0, "path should end in /ls, got %s", p);
		CHECK(access(p, X_OK) == 0, "returned path should be executable: %s", p);
	}
	free(p);

	p = search_path("definitely_not_a_command_xyz");
	CHECK(p == NULL, "unknown command should return NULL, got %s", p ? p : "");
	free(p);
}

/* The search rules from the spec, using the fake filesystem from setup(). */
static void test_search_rules(void)
{
	/* found in the only directory that has it */
	set_path(dir_a, dir_b);
	char *p = search_path("local");
	CHECK_STR(p, join(dir_b, "local"));
	free(p);

	/* first match in PATH order wins */
	p = search_path("both");
	CHECK_STR(p, join(dir_a, "both"));
	free(p);

	set_path(dir_b, dir_a);
	p = search_path("both");
	CHECK_STR(p, join(dir_b, "both"));
	free(p);

	/* a non-executable file is skipped in favor of a later executable one */
	set_path(dir_a, dir_b);
	p = search_path("only_b");
	CHECK_STR(p, join(dir_b, "only_b"));
	free(p);

	/* a non-executable file with no other match is "not found" */
	set_path(dir_a, NULL);
	p = search_path("only_b");
	CHECK(p == NULL, "non-executable file should not be returned, got %s", p ? p : "");
	free(p);

	/* a directory is not a program (note: access(dir, X_OK) succeeds on directories!) */
	p = search_path("subdir");
	CHECK(p == NULL, "a directory should not be returned, got %s", p ? p : "");
	free(p);

	/* "." in PATH means the current directory (the spec's example PATH ends with ".") */
	char old_cwd[PATH_MAX];
	if (getcwd(old_cwd, sizeof(old_cwd)) != NULL && chdir(dir_b) == 0) {
		set_path("/nonexistent_dir", ".");
		p = search_path("local");
		CHECK_STR(p, "./local");
		free(p);
		if (chdir(old_cwd) != 0)
			perror("chdir");
	}

	/* nonexistent directories in PATH are skipped without error */
	set_path("/nonexistent_dir", dir_b);
	p = search_path("local");
	CHECK_STR(p, join(dir_b, "local"));
	free(p);
}

/* spec: "For commands that do not include a slash (/) ... search each directory".
 * A command with a slash is already a path (absolute or relative): no search. */
static void test_slash_commands(void)
{
	/* commands containing '/' are returned as-is, without searching */
	setenv("PATH", "/usr/bin:/bin", 1);

	char *p = search_path("/bin/ls");
	CHECK_STR(p, "/bin/ls");
	free(p);

	p = search_path("./bin/shell");
	CHECK_STR(p, "./bin/shell");
	free(p);

	p = search_path("../some/prog");
	CHECK_STR(p, "../some/prog");
	free(p);
}

/* strtok writes '\0' into the string it splits; doing that to getenv("PATH") would
 * cut the shell's real PATH down to its first directory for the rest of the session. */
static void test_path_not_modified(void)
{
	/* searching must not modify the PATH string (e.g. strtok on getenv's memory) */
	set_path(dir_a, dir_b);
	char *before = strdup(getenv("PATH"));

	char *p = search_path("local");
	free(p);
	p = search_path("definitely_not_a_command_xyz");
	free(p);

	CHECK_STR(getenv("PATH"), before);
	free(before);
}

/* Inputs that must not crash the shell. */
static void test_edge_cases(void)
{
	/* PATH unset: nothing can be found, but no crash */
	unsetenv("PATH");
	char *p = search_path("ls");
	CHECK(p == NULL, "with PATH unset, ls should not be found, got %s", p ? p : "");
	free(p);

	/* empty command name */
	setenv("PATH", "/usr/bin:/bin", 1);
	p = search_path("");
	CHECK(p == NULL, "empty command should return NULL, got %s", p ? p : "");
	free(p);
}

int main(void)
{
	if (setup() != 0)
		return 1;

	test_real_commands();
	test_search_rules();
	test_slash_commands();
	test_path_not_modified();
	test_edge_cases();

	teardown();
	return TEST_SUMMARY("test_path_search");
}
