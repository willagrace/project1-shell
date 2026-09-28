#include "lexer.h"
#include "prompt.h"
#include "jobs.h"
#include "builtins.h"
#include "expand.h"
#include "execute.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    jobs_init();

    while (1) {

        /*
         * Part 8:
         * Check whether previous background
         * processes have finished.
         */
        check_background_jobs();

        /*
         * Part 1
         */
        print_prompt();

        /*
         * Starter lexer
         */
        char *input = get_input();

        if (input == NULL) {
            break;
        }

        tokenlist *tokens = get_tokens(input);
        /*
         * Ignore empty commands.
         */
        if (tokens->size == 0) {
            free(input);
            free_tokens(tokens);
            continue;
        }
        // Parts 2&3: Expand environment variables and tilde (~) in tokens.
        expand_tokens(tokens);
        /*
         * Part 9
         */
        if (is_builtin(tokens)) {

            int result = execute_builtin(tokens);

            if (result == SHELL_EXIT) {
                free(input);
                free_tokens(tokens);
                break;
            }

            /*
             * Successful built-in command.
             */
            if (result == 0) {
                add_to_history(input);
            }

            free(input);
            free_tokens(tokens);

            continue;
        }

        /*
         * Parts 4-8: search $PATH for each command, then fork/execv it with
         * any redirection, pipes, and background processing.
         */
        if (execute_command(tokens, input) == 0) {
            add_to_history(input);
        }


        free(input);
        free_tokens(tokens);

    }

    return 0;
}
