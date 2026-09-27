#pragma once

#include "lexer.h"

// Expands tokens with expand_env_vars and expand_tilde functions.
tokenlist * expand_tokens(tokenlist *tokens);

// Expands tokens in a tokenlist by replacing any env variables with their values. 
// If an env variable is not found, it will be replaced with an empty string.
tokenlist * expand_env_vars(tokenlist *tokens);

// Expands the tilde (~) to the $HOME environment variable.
// If a token does not start with a tilde, it will be returned unchanged.
tokenlist * expand_tilde(tokenlist *tokens);