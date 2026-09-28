#include "jobs.h"

#include <stdio.h>
#include <stdlib.h>
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

int add_background_job(pid_t pid, const char *command) {
    return add_background_pipeline(&pid, 1, command);
}

int add_background_pipeline(const pid_t *pids, size_t num_pids, const char *command)
{
    int i;

    for (i = 0; i < MAX_JOBS; i++) {

        if (!jobs[i].active) {

            jobs[i].job_number = next_job_number++;
            jobs[i].pid = pids[num_pids - 1];
            jobs[i].num_pids = num_pids;
            jobs[i].num_running = num_pids;
            jobs[i].active = true;
            
            jobs[i].pids = malloc(num_pids * sizeof(pid_t));
            memcpy(jobs[i].pids, pids, num_pids * sizeof(pid_t));

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
    size_t p;
    int status;
    pid_t result;

    for (i = 0; i < MAX_JOBS; i++) {

        if (!jobs[i].active) {
            continue;
        }
        for (p = 0; p < jobs[i].num_pids; p++) {
            if (jobs[i].pids[p] == 0) {
                continue;
            }

            result = waitpid(
                jobs[i].pids[p],
                &status,
                WNOHANG
            );

            if (result == jobs[i].pids[p]) {
                jobs[i].pids[p] = 0;
                jobs[i].num_running--;
            }
        }

        if (jobs[i].num_running == 0) {

            printf(
                "[%d] + done %s\n",
                jobs[i].job_number,
                jobs[i].command
            );

            free(jobs[i].pids);
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
    size_t p;
    int status;

    for (i = 0; i < MAX_JOBS; i++) {

        if (jobs[i].active) {

            for (p = 0; p < jobs[i].num_pids; p++) {
                if (jobs[i].pids[p] == 0) {
                    continue;
                }

                waitpid(
                    jobs[i].pids[p],
                    &status,
                    0
                );
            }

            free(jobs[i].pids);
            jobs[i].active = false;
        }
    }
}
