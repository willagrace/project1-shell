#include "lexer.h"
#include "expand.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

tokenlist * expand_env_vars(tokenlist *tokens) {

	for (size_t i = 0; i < tokens->size; i++) {
		char *token = tokens->items[i];

		if (strcmp(token, "$") == 0) {
			continue;
		}

		if (token[0] == '$') {
			char *env_var = getenv(token + 1);
			free(tokens->items[i]);

			if (env_var != NULL) {
				tokens->items[i] = (char *)malloc(strlen(env_var) + 1);
				strcpy(tokens->items[i], env_var);
			} else {
				tokens->items[i] = (char *)malloc(1);
				tokens->items[i][0] = '\0';
			}

		}
	}
	return tokens;
}

tokenlist * expand_tilde(tokenlist *tokens) {
	char *home_dir = getenv("HOME");
	if (home_dir == NULL) {
		return tokens;
	}


	for (size_t i = 0; i < tokens->size; i++) {
		char *token = tokens->items[i];

		if (token[0] == '~' && (token[1] == '\0' || token[1] == '/')) {
			size_t new_size = strlen(home_dir) + strlen(token + 1) + 1;
			char *expanded_token = (char *)malloc(new_size);
			strcpy(expanded_token, home_dir);
			strcat(expanded_token, token + 1);
			free(tokens->items[i]);
			tokens->items[i] = expanded_token;
		}
	}

	return tokens;
}

tokenlist * expand_tokens(tokenlist *tokens) {
	expand_tilde(tokens);
	expand_env_vars(tokens);
	return tokens;
}
