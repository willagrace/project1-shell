#define _XOPEN_SOURCE 700

#include "prompt.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


/*
 *In the beginning we are making two different arrays:one for the machine name
 *and the other forthe current directory. We're grabbing the info the value from 
 * environmental variable USER and placing it in the variable user. Overall, we are
 * grabbing the info of the machine's name, the host's name, and the current working directory and we are 
 * printing it. Before we print anything we are checking to see if the regular variable is null,
 * checking if the hostname is anything besides -1, and checking if the current working directory
 * is also null. 
 * */



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
