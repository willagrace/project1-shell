#define _XOPEN_SOURCE 700

#include "prompt.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void print_prompt(void)
{
    char hostname[256];
    char cwd[4096];

    const char *user = getenv("USER");

    if (user == NULL) {
        user = "unknown";
    }

    if (gethostname(hostname, sizeof(hostname)) == -1) {
        snprintf(hostname, sizeof(hostname), "unknown");
    }

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        snprintf(cwd, sizeof(cwd), "unknown");
    }

    printf("%s@%s:%s> ", user, hostname, cwd);
    fflush(stdout);
}
