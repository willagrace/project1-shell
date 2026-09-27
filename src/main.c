#include "lexer.h"
#include "prompt.h"
#include "jobs.h"
#include "builtins.h"

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
         * ===================================
         * TEAMMATES' CODE GOES HERE
         * ===================================
         *
         * Part 2: Environment variables
         * Part 3: Tilde expansion
         * Part 4: PATH search
         * Part 5: External execution
         * Part 6: I/O redirection
         * Part 7: Pipes
         *
         *
         * Once Part 5 forks:
         *
         * if background:
         *
         *     add_background_job(pid, input);
         *
         * otherwise:
         *
         *     waitpid(pid, NULL, 0);
         *
         *
         * Once the command is known to be valid:
         *
         *     add_to_history(input);
         */

        free(input);
        free_tokens(tokens);
    }

    return 0;
}
