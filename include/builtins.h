#pragma once

#include "lexer.h"

#include <stdbool.h>

#define SHELL_EXIT -1

bool is_builtin(tokenlist *tokens);
int execute_builtin(tokenlist *tokens);

void add_to_history(const char *command);
