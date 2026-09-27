#pragma once

// Returns a fresh copy of the path to the executable for the cmd (or NULL if not found).
// Commands with '/' are returned as copy without searching the PATH.
char *search_path(const char *cmd);
