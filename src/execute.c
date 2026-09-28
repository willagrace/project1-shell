#define _XOPEN_SOURCE 700

#include "execute.h"
#include "jobs.h"
#include "path_search.h"

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

// one command in a pipeline 
typedef struct {
    char **argv;    /* NULL-terminated argument list for execv() */
    size_t argc;
    char *path;     /* full path from search_path() (malloc'd) */
    char *infile;   /* file name after "<", or NULL */
    char *outfile;  /* file name after ">", or NULL */
    int in_fd;      /* opened infile, or -1 */
    int out_fd;     /* opened outfile, or -1 */
} command;


static void close_fd(int fd){
    if (fd != -1) {
        close(fd);
    }
}

static void close_redirections(command *cmds, size_t count) {
    size_t i;

    for (i = 0; i < count; i++) {
        close_fd(cmds[i].in_fd);
        close_fd(cmds[i].out_fd);
        cmds[i].in_fd = -1;
        cmds[i].out_fd = -1;
    }
}

static void free_commands(command *cmds, size_t count) {
    size_t i;

    close_redirections(cmds, count);

    for (i = 0; i < count; i++) {
        free(cmds[i].argv);
        free(cmds[i].path);
    }
    free(cmds);
}

static bool is_operator(const char *token) {
    return strcmp(token, "|") == 0 || 
           strcmp(token, "<") == 0 || 
           strcmp(token, ">") == 0;
}

// split a list of tokens into commands
static size_t parse_commands(tokenlist *tokens, size_t num_tokens, command **result)
{
    size_t count = 1;
    size_t current = 0;
    size_t i;
    bool valid = true;
    command *cmds;

    for (i = 0; i < num_tokens; i++) {
        if (strcmp(tokens->items[i], "|") == 0) {
            count++;
        }
    }

    cmds = calloc(count, sizeof(command));

    for (i = 0; i < count; i++) {
        cmds[i].argv = calloc(num_tokens + 1, sizeof(char *));
        cmds[i].in_fd = -1;
        cmds[i].out_fd = -1;
    }

    for (i = 0; i < num_tokens && valid; i++) {

        char *token = tokens->items[i];

        if (strcmp(token, "|") == 0) {
            /* "| ls" or "ls | | wc" has an empty command */
            if (cmds[current].argc == 0) {
                valid = false;
            }
            current++;
        }
        else if (strcmp(token, "<") == 0 || strcmp(token, ">") == 0) {
            /* the file name is the next token */
            if (i + 1 >= num_tokens || is_operator(tokens->items[i + 1])) {
                valid = false;
            }
            else if (token[0] == '<') {
                cmds[current].infile = tokens->items[++i];
            }
            else {
                cmds[current].outfile = tokens->items[++i];
            }
        }
        else {
            cmds[current].argv[cmds[current].argc++] = token;
        }
    }

    /* catches "ls |" and a line that is only "< file" */
    if (cmds[count - 1].argc == 0) {
        valid = false;
    }

    if (!valid) {
        fprintf(stderr, "syntax error\n");
        free_commands(cmds, count);
        return 0;
    }

    *result = cmds;
    return count;
}


// Open input file (check it exists and is a regular file)
static int open_input_file(const char *file) {
    struct stat info;
    int fd;

    if (stat(file, &info) == -1) {
        fprintf(stderr, "%s: No such file or directory\n", file);
        return -1;
    }
    if (!S_ISREG(info.st_mode)) {
        fprintf(stderr, "%s: Not a regular file\n", file);
        return -1;
    }

    fd = open(file, O_RDONLY);

    if (fd == -1) {
        perror(file);
    }

    return fd;
}


// Open output file and create one if needed
static int open_output_file(const char *file)
{
    int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

    if (fd == -1) {
        perror(file);
        return -1;
    }

    fchmod(fd, S_IRUSR | S_IWUSR);

    return fd;
}



static bool open_redirections(command *cmds, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++) {

        if (cmds[i].infile != NULL) {
            cmds[i].in_fd = open_input_file(cmds[i].infile);
            if (cmds[i].in_fd == -1) {
                return false;
            }
        }

        if (cmds[i].outfile != NULL) {
            cmds[i].out_fd = open_output_file(cmds[i].outfile);
            if (cmds[i].out_fd == -1) {
                return false;
            }
        }
    }

    return true;
}




/*
 * Fork one child per command, connecting each command's stdout to the next
 * command's stdin with a pipe. A redirection file takes priority over a pipe.
 */
static void run_pipeline(command *cmds, size_t count, bool background,
                         const char *command_line)
{
    pid_t *pids = calloc(count, sizeof(pid_t));
    size_t started = 0;
    int prev_read = -1;     /* read end of the pipe from the previous command */
    size_t i;

    for (i = 0; i < count; i++) {

        int pipe_fds[2] = { -1, -1 };
        pid_t pid;

        /* every command except the last writes into a new pipe */
        if (i < count - 1 && pipe(pipe_fds) == -1) {
            perror("pipe");
            break;
        }

        /* don't let the child inherit (and later repeat) unprinted output */
        fflush(stdout);

        pid = fork();

        if (pid == -1) {
            perror("fork");
            close_fd(pipe_fds[0]);
            close_fd(pipe_fds[1]);
            break;
        }

        if (pid == 0) {
            /* ----- child ----- */
            int in_fd = (cmds[i].in_fd != -1) ? cmds[i].in_fd : prev_read;
            int out_fd = (cmds[i].out_fd != -1) ? cmds[i].out_fd : pipe_fds[1];

            if (in_fd != -1) {
                dup2(in_fd, STDIN_FILENO);
            }

            if (out_fd != -1) {
                dup2(out_fd, STDOUT_FILENO);
            }

            /*
             * stdin/stdout now point where they should. Close every other
             * copy, or a reader down the pipeline never sees end-of-file.
             */
            close_fd(prev_read);
            close_fd(pipe_fds[0]);
            close_fd(pipe_fds[1]);
            close_redirections(cmds, count);

            execv(cmds[i].path, cmds[i].argv);

            /* execv only returns if it failed */
            perror(cmds[i].argv[0]);
            _exit(1);
        }

        /* ----- parent ----- */
        pids[started++] = pid;

        /* the parent doesn't use the pipes, it only passes them along */
        close_fd(prev_read);
        close_fd(pipe_fds[1]);
        prev_read = pipe_fds[0];
    }

    close_fd(prev_read);
    close_redirections(cmds, count);

    if (background && started > 0) {
        add_background_pipeline(pids, started, command_line);
    }
    else {
        for (i = 0; i < started; i++) {
            waitpid(pids[i], NULL, 0);
        }
    }

    free(pids);
}


int execute_command(tokenlist *tokens, const char *command_line)
{
    bool background = is_background(tokens);
    size_t num_tokens = background ? tokens->size - 1 : tokens->size;
    command *cmds;
    size_t count;
    size_t i;

    count = parse_commands(tokens, num_tokens, &cmds);

    if (count == 0) {
        return 1;
    }

    /* Part 4: find every program before starting any of them */
    for (i = 0; i < count; i++) {

        cmds[i].path = search_path(cmds[i].argv[0]);

        if (cmds[i].path == NULL) {
            fprintf(stderr, "%s: command not found\n", cmds[i].argv[0]);
            free_commands(cmds, count);
            return 1;
        }
    }

    /* Part 6 */
    if (!open_redirections(cmds, count)) {
        free_commands(cmds, count);
        return 1;
    }

    /* Parts 5, 7, 8 */
    run_pipeline(cmds, count, background, command_line);

    free_commands(cmds, count);

    return 0;
}