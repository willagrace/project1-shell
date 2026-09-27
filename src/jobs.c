#include "jobs.h"

#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static Job jobs[MAX_JOBS];
static int next_job_number = 1;

void jobs_init(void)
{
    int i;

    for (i = 0; i < MAX_JOBS; i++) {
        jobs[i].active = false;
    }
}

bool is_background(tokenlist *tokens)
{
    if (tokens == NULL || tokens->size == 0) {
        return false;
    }

    return strcmp(tokens->items[tokens->size - 1], "&") == 0;
}

int add_background_job(pid_t pid, const char *command)
{
    int i;

    for (i = 0; i < MAX_JOBS; i++) {

        if (!jobs[i].active) {

            jobs[i].job_number = next_job_number++;
            jobs[i].pid = pid;
            jobs[i].active = true;

            strncpy(
                jobs[i].command,
                command,
                MAX_COMMAND_LENGTH - 1
            );

            jobs[i].command[MAX_COMMAND_LENGTH - 1] = '\0';

            printf(
                "[%d] %d\n",
                jobs[i].job_number,
                (int)jobs[i].pid
            );

            return jobs[i].job_number;
        }
    }

    fprintf(stderr, "Error: too many background jobs\n");

    return -1;
}

void check_background_jobs(void)
{
    int i;
    int status;
    pid_t result;

    for (i = 0; i < MAX_JOBS; i++) {

        if (!jobs[i].active) {
            continue;
        }

        result = waitpid(
            jobs[i].pid,
            &status,
            WNOHANG
        );

        if (result == jobs[i].pid) {

            printf(
                "[%d]+ done %s\n",
                jobs[i].job_number,
                jobs[i].command
            );

            jobs[i].active = false;
        }
    }
}

void print_jobs(void)
{
    int i;
    bool found = false;

    for (i = 0; i < MAX_JOBS; i++) {

        if (jobs[i].active) {

            printf(
                "[%d]+ %d %s\n",
                jobs[i].job_number,
                (int)jobs[i].pid,
                jobs[i].command
            );

            found = true;
        }
    }

    if (!found) {
        printf("No active background processes.\n");
    }
}

void wait_for_background_jobs(void)
{
    int i;
    int status;

    for (i = 0; i < MAX_JOBS; i++) {

        if (jobs[i].active) {

            waitpid(
                jobs[i].pid,
                &status,
                0
            );

            jobs[i].active = false;
        }
    }
}
