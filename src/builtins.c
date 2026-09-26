#include "builtins.h"
#include "jobs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HISTORY_SIZE 3
#define MAX_HISTORY_COMMAND_LENGTH 200

static char history[HISTORY_SIZE][MAX_HISTORY_COMMAND_LENGTH];
static int history_count = 0;


/*
 * Add a valid command to our three-command history.
 */
void add_to_history(const char *command)
{
    if (command == NULL || command[0] == '\0') {
        return;
    }

    if (history_count < HISTORY_SIZE) {

        strncpy(
            history[history_count],
            command,
            MAX_HISTORY_COMMAND_LENGTH - 1
        );

        history[history_count][MAX_HISTORY_COMMAND_LENGTH - 1] = '\0';

        history_count++;
    }
    else {

        strcpy(history[0], history[1]);
        strcpy(history[1], history[2]);

        strncpy(
            history[2],
            command,
            MAX_HISTORY_COMMAND_LENGTH - 1
        );

        history[2][MAX_HISTORY_COMMAND_LENGTH - 1] = '\0';
    }
}


static void print_history(void)
{
    int i;

    if (history_count == 0) {
        printf("No valid commands.\n");
        return;
    }

    printf("Last valid commands:\n");

    for (i = 0; i < history_count; i++) {
        printf("%s\n", history[i]);
    }
}


static int builtin_cd(tokenlist *tokens)
{
    const char *path;

    /*
     * cd can have at most one argument.
     */
    if (tokens->size > 2) {
        fprintf(stderr, "cd: too many arguments\n");
        return 1;
    }

    /*
     * Plain "cd" goes to HOME.
     */
    if (tokens->size == 1) {

        path = getenv("HOME");

        if (path == NULL) {
            fprintf(stderr, "cd: HOME is not set\n");
            return 1;
        }
    }
    else {
        path = tokens->items[1];
    }

    /*
     * chdir fails if the path doesn't exist or
     * cannot be changed into.
     */
    if (chdir(path) == -1) {
        perror("cd");
        return 1;
    }

    return 0;
}


static int builtin_jobs(void)
{
    check_background_jobs();
    print_jobs();

    return 0;
}


static int builtin_exit(void)
{
    /*
     * Assignment requires us to wait for
     * background processes before exiting.
     */
    wait_for_background_jobs();

    print_history();

    return SHELL_EXIT;
}


bool is_builtin(tokenlist *tokens)
{
    if (tokens == NULL || tokens->size == 0) {
        return false;
    }

    if (strcmp(tokens->items[0], "cd") == 0) {
        return true;
    }

    if (strcmp(tokens->items[0], "jobs") == 0) {
        return true;
    }

    if (strcmp(tokens->items[0], "exit") == 0) {
        return true;
    }

    return false;
}


int execute_builtin(tokenlist *tokens)
{
    if (tokens == NULL || tokens->size == 0) {
        return 0;
    }

    if (strcmp(tokens->items[0], "cd") == 0) {
        return builtin_cd(tokens);
    }

    if (strcmp(tokens->items[0], "jobs") == 0) {
        return builtin_jobs();
    }

    if (strcmp(tokens->items[0], "exit") == 0) {
        return builtin_exit();
    }

    return 1;
}
