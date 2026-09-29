# Shell

Project 1 for Section 1 of Operating Systems Fall 2026.

## Group Members
- **Terryon Larkins**: tl17f@fsu.edu
- **Willa Gutowski**: wgg22@fsu.edu
- **Carter Rudolph**: cgr24c@fsu.edu
## Division of Labor

### Part 1: Prompt
- **Responsibilities**: Print `USER@MACHINE:PWD> ` before each command (`prompt.c`).
- **Assigned to**: Terryon Larkins

### Part 2: Environment Variables
- **Responsibilities**: Replace tokens of the form `$NAME` with the variable's value
  (`expand.c`).
- **Assigned to**: Carter Rudolph

### Part 3: Tilde Expansion
- **Responsibilities**: Replace `~` and a leading `~/` with `$HOME` (`expand.c`).
- **Assigned to**: Carter Rudolph

### Part 4: $PATH Search
- **Responsibilities**: Find the executable for a command by searching the directories
  in `$PATH`; use commands containing `/` as given; report "command not found"
  (`path_search.c`).
- **Assigned to**: Carter Rudolph

### Part 5: External Command Execution
- **Responsibilities**: Fork a child process and run the command with `execv`, passing
  all arguments; the shell waits for it to finish (`execute.c`).
- **Assigned to**: Willa Gutowski

### Part 6: I/O Redirection
- **Responsibilities**: Redirect a command's input from a file (`<`) and output to a file
  (`>`, created or overwritten with `-rw-------`) (`execute.c`).
- **Assigned to**: Willa Gutowski

### Part 7: Piping
- **Responsibilities**: Connect up to three commands with pipes (`|`) so each command's
  output is the next command's input (`execute.c`).
- **Assigned to**: Willa Gutowski

### Part 8: Background Processing
- **Responsibilities**: Track background jobs, print `[N] PID` on start and
  `[N]+ done CMD` on completion (`jobs.c`).
- **Assigned to**: Terryon Larkins

### Part 9: Internal Command Execution
- **Responsibilities**: Built-ins `cd`, `jobs`, and `exit` with command history
  (`builtins.c`).
- **Assigned to**: Terryon Larkins

### Extra Credit
- **Responsibilities**: Support unlimited pipes, support piping and I/O redirection in one
  command, and run the shell from within a running shell repeatedly (`execute.c`).
- **Assigned to**: Willa Gutowski

## File Listing
```
project1-shell/
├── Makefile                # build rules: make, make run, make test, make clean
├── README.md               # this file
├── compile_flags.txt       # compiler flags for editor tooling (clangd)
├── .gitignore              # keeps bin/, obj/, and editor files out of the repository
├── bin/                    # executables (created by make; not committed)
├── obj/                    # object files (created by make; not committed)
├── include/
│   ├── builtins.h          # built-in command interface (Part 9)
│   ├── execute.h           # external command execution interface (Parts 5-7)
│   ├── expand.h            # $VAR and ~ expansion interface (Parts 2, 3)
│   ├── jobs.h              # background job table interface (Part 8)
│   ├── lexer.h             # tokenlist type and tokenizer (starter code)
│   ├── path_search.h       # $PATH search interface (Part 4)
│   └── prompt.h            # prompt interface (Part 1)
├── src/
│   ├── builtins.c          # cd, jobs, exit, and the last-three-commands history
│   ├── execute.c           # fork/execv, I/O redirection, and pipes
│   ├── expand.c            # replaces $NAME tokens and ~ / ~/ prefixes
│   ├── jobs.c              # starts, lists, and reaps background jobs
│   ├── lexer.c             # reads a line and splits it into tokens (starter code)
│   ├── main.c              # main loop: prompt, read, expand, built-ins, execution
│   ├── path_search.c       # finds a command's executable in $PATH
│   └── prompt.c            # prints USER@MACHINE:PWD>
└── tests/                  # test suite (run with make test)
    ├── integration.sh      # end-to-end tests that run bin/shell for every part
    ├── test.h              # shared check and output-capture helpers
    ├── test_builtins.c     # unit tests for Part 9
    ├── test_expand.c       # unit tests for Parts 2 and 3
    ├── test_jobs.c         # unit tests for Part 8
    ├── test_path_search.c  # unit tests for Part 4
    └── test_prompt.c       # unit tests for Part 1
```
## How to Compile & Execute

### Requirements
- **Compiler**: `gcc` (C99) and `make`
- **Dependencies**: None

### Compilation
```bash
make
```
This will build the executable in `bin/shell`.
### Execution
```bash
make run
```
This will run the program `bin/shell`. It can also be started directly with
`./bin/shell`.

## Development Log
Each member records their contributions here. Use of AI is detailed in each developer's log.

### Willa Gutowski

| Date       | Work Completed / Notes |
|------------|------------------------|
| 2026-09-20 | Planned tasks, reviewed C, etc. no project coding |
| 2026-09-26-27 | Implemented 5, 6, 7   |
| 2026-09-28 | Implemented EC and merged  |

**Use of AI:** I used Claude Opus and Sonnet for concept review and code idea generation, but did
not invite Claude as a collaborator on the repo. I spent time reviewing integrating the sensible
output into my files, and thinking about how my changes would interact with teammate's code
before committing/pushing.

### Terryon Larkins

| Date       | Work Completed / Notes |
|------------|------------------------|
| 2026-09-26 | Implemented Parts 1, 8, and 9.   |
| 2026-09-28 | Wrote notes for the implementation of parts 1, 8, and 9  |
| 2026-09-28 | Merged notes into main branch |

**Use of AI:** I used ChatGPT to generate the first draft of code for my parts. I have been
using the test to check my own work. In addition, to be very specific, I used google's ai as a
cheatsheet for git commands, since I'm not entirely comfortable with them. But I also tried using
git's own official cheat sheet when I could. Before submission, I manually checked if my specific
portions worked as intended. Lastly, used AI, textbook, and module notes to check over my
understanding of my functions and parts.


### Carter Rudolph

| Date       | Work Completed / Notes |
|------------|------------------------|
| 2026-09-20 | Planned the work for Parts 2-4. No coding. |
| 2026-09-26 | Implemented Parts 2 and 3 (`expand.c`). |
| 2026-09-27 | Merged Terryon's Parts 1, 8, and 9. Implemented Part 4 (`path_search.c`). |
| 2026-09-27 | Connected Parts 2-4 in `main.c`. Wrote documentation. |

**Use of AI:** While developing, I used AI (Claude) to review concepts, check my
code/readability, and generate the test suite in `tests/`. I also used Claude to build and test
the code in a Fedora 44 Docker container, since linprog runs Fedora 44; this is only a
preliminary check, not a substitute for testing on linprog. Claude created the first draft of
this README.


## Meetings
Document in-person meetings, their purpose, and what was discussed.

| Date       | Attendees            | Topics Discussed | Outcomes / Decisions |
|------------|----------------------|------------------|-----------------------|
| 2026-09-08 | Terryon, Willa, Carter | Division of labor | Wrote the division of labor document |
| 2026-09-17 | Terryon, Willa, Carter | Limited meeting times | Redid the division of labor |
| 2026-09-28 | Terryon, Willa, Carter | (Planned) Review project | Test before submission |



## Bugs
No known bugs!

## Extra Credit
- **Extra Credit 1**: Support for unlimited piping. `execute.c` forks one child per command, so
  any number of pipes works, e.g. `echo a | cat | cat | cat | tr a b`.
- **Extra Credit 2**: Support for I/O redirection and unlimited piping in one command. Each
  command in a pipeline can have its own `<` or `>`, and a redirection file takes priority over
  the pipe, e.g. `cat < in.txt | sort | uniq > out.txt`.
- **Extra Credit 3**: Execute the shell from within a running shell, e.g. run `./bin/shell` at
  the prompt; `exit` returns to the outer shell.

## Considerations
We had to reevaluate the division of labor because time constraints limited our ability
to meet as a group. We used AI to generate unit and integration tests (with clear
documentation) for extra testing.
