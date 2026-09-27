#define _POSIX_C_SOURCE 200809L

#include "path_search.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

// Returns true if path is a regular file (not a directory) the user may execute.
static int is_executable(const char *path) {
	struct stat sb;
	return stat(path, &sb) == 0 && S_ISREG(sb.st_mode) && access(path, X_OK) == 0;
}

char *search_path(const char *cmd) {
	if (cmd == NULL || cmd[0] == '\0') {
		return NULL;
	}

	// A command containing '/' is already a path: use it as-is. If it can't be run,
	// execv reports the real reason (not found, permission denied, ...).
	if (strchr(cmd, '/') != NULL) {
		return strdup(cmd);
	}

	// Get the PATH environment variable.
	const char *path_env = getenv("PATH");
	if (path_env == NULL) {
		return NULL;
	}

	// Tokenize the PATH variable and search for the executable.
	char *path_copy = strdup(path_env);
	char *saveptr;
	char *dir = strtok_r(path_copy, ":", &saveptr);
	while (dir != NULL) {
		size_t len = strlen(dir) + strlen(cmd) + 2; // +1 for '/' and +1 for '\0'
		char *full_path = (char *)malloc(len);
		snprintf(full_path, len, "%s/%s", dir, cmd);

		if (is_executable(full_path)) {
			free(path_copy);
			return full_path; // Found the executable.
		}

		free(full_path);
		dir = strtok_r(NULL, ":", &saveptr);
	}

	free(path_copy);
	return NULL; // Not found in any PATH directory.
}
