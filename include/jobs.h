#pragma once

#include "lexer.h"

#include <stdbool.h>
#include <sys/types.h>

#define MAX_JOBS 10
#define MAX_COMMAND_LENGTH 200

typedef struct {
    int job_number;
    pid_t pid;
    pid_t *pids;
    size_t num_pids;
    size_t num_running;
    char command[MAX_COMMAND_LENGTH];
    bool active;
} Job;

void jobs_init(void);

bool is_background(tokenlist *tokens);

int add_background_job(pid_t pid, const char *command);

int add_background_pipeline(const pid_t *pids, size_t num_pids, const char *command);

void check_background_jobs(void);
void print_jobs(void);
void wait_for_background_jobs(void);
