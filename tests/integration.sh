#!/usr/bin/env bash
# integration.sh - end-to-end tests for every part of the project.
#
# The unit tests (tests/test_*.c) check each module by itself; this script runs the
# real bin/shell the way a user or grader would, so it also catches mistakes in how
# main.c connects the modules (for example, expanding tokens after the built-in check).
#
# How: each test writes a few command lines (plus a final "exit") to a file, feeds it
# to bin/shell on stdin, and checks the output. Every test runs in a fresh, empty
# scratch directory with USER and HOME set to known values, and a watchdog kills the
# shell if it hangs. Prompts are stripped from stdout before output is compared.
#
# Failing checks are labeled with the part they belong to, e.g. "FAIL [6] ...".
#
# Usage: bash tests/integration.sh [path/to/shell]   (from the project root; make test
#        runs it automatically)

SHELL_BIN="$(cd "$(dirname "${1:-bin/shell}")" && pwd -P)/$(basename "${1:-bin/shell}")"
if [ ! -x "$SHELL_BIN" ]; then
	echo "integration: $SHELL_BIN not found; run make first"
	exit 1
fi

# pwd -P resolves symlinks (on macOS /tmp -> /private/tmp) so paths match getcwd()
TMP="$(cd "$(mktemp -d /tmp/shell_it_XXXXXX)" && pwd -P)"
WORK="$TMP/work"
TEST_HOME="$TMP/home"
TEST_USER="tester"
TIMEOUT=10
pass=0
fail=0

cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

# Start each test with an empty working directory and home directory.
fresh() {
	rm -rf "$WORK" "$TEST_HOME"
	mkdir -p "$WORK" "$TEST_HOME"
}

# run_shell "line1\nline2" : runs the lines followed by "exit".
# Sets OUT (stdout with prompts removed), RAW (stdout as-is), ERR (stderr), SECS.
run_shell() {
	printf '%b\nexit\n' "$1" > "$TMP/in"
	local start end
	start=$(date +%s)
	(
		cd "$WORK" || exit 1
		ulimit -f 2048 # stop a runaway loop from filling the disk
		USER="$TEST_USER" HOME="$TEST_HOME" exec "$SHELL_BIN" \
			< "$TMP/in" > "$TMP/out" 2> "$TMP/err"
	) &
	local pid=$!
	( sleep "$TIMEOUT"; kill -9 "$pid" 2> /dev/null ) &
	local watchdog=$!
	wait "$pid" 2> /dev/null
	kill "$watchdog" 2> /dev/null
	wait "$watchdog" 2> /dev/null
	end=$(date +%s)
	SECS=$((end - start))

	RAW="$(cat "$TMP/out")"
	ERR="$(cat "$TMP/err")"
	# remove every "user@host:/path> " prompt, leaving only command output
	OUT="$(sed -E "s#${TEST_USER}@[^:]*:[^>]*> ##g" "$TMP/out")"
}

# Record a passing check.
ok() {
	pass=$((pass + 1))
}

# Record a failing check: bad PART DESCRIPTION [DETAILS]
bad() {
	fail=$((fail + 1))
	echo "  FAIL [$1] $2"
	[ -n "$3" ] && echo "$3" | sed 's/^/        /' | head -8
}

# expect PART DESCRIPTION CONDITION... : passes if the condition command succeeds
expect() {
	local part="$1" desc="$2"
	shift 2
	if "$@"; then ok; else bad "$part" "$desc" "stdout: $OUT
stderr: $ERR"; fi
}

# Conditions used with expect (OUT/ERR come from the most recent run_shell):
#   out_has TEXT    stdout contains TEXT          out_line REGEX  a whole line matches
#   out_lacks TEXT  stdout does not contain TEXT  raw_has TEXT    prompt-included output
#   err_has TEXT    stderr contains TEXT          err_(non)empty  stderr (non)empty
#   file_is F TEXT  work file F contains TEXT     perms_are F P   ls -l permissions of F
out_has() { printf '%s\n' "$OUT" | grep -qF -- "$1"; }
out_line() { printf '%s\n' "$OUT" | grep -qxE -- "$1"; }
out_lacks() { ! printf '%s\n' "$OUT" | grep -qF -- "$1"; }
raw_has() { printf '%s\n' "$RAW" | grep -qF -- "$1"; }
err_has() { printf '%s\n' "$ERR" | grep -qiF -- "$1"; }
err_nonempty() { [ -n "$ERR" ]; }
err_empty() { [ -z "$ERR" ]; }
file_is() { [ -f "$WORK/$1" ] && [ "$(cat "$WORK/$1")" = "$2" ]; }
perms_are() { [ "$(ls -l "$WORK/$1" | cut -c1-10)" = "$2" ]; }

# ---------------------------------------------------------------- Part 1
# Prompt: "USER@MACHINE:PWD> " with the absolute working directory.
fresh
run_shell ""
expect 1 "prompt is USER@MACHINE:PWD> " raw_has "$TEST_USER@$(hostname):$WORK> "

# ---------------------------------------------------------------- Part 2
# Environment variables: "$NAME" tokens are replaced with their values in any command.
fresh
run_shell 'echo $USER'
expect 2 "echo \$USER prints the user" out_line "$TEST_USER"

fresh
run_shell 'echo $HOME $USER'
expect 2 "several variables in one command" out_line "$TEST_HOME $TEST_USER"

fresh
run_shell 'echo $DEFINITELY_UNSET_VAR_XYZ'
expect 2 "unset variable prints nothing" err_empty

# ---------------------------------------------------------------- Part 3
# Tilde expansion: "~" and "~/..." become $HOME; other uses of ~ are left alone.
fresh
run_shell 'echo ~'
expect 3 "echo ~ prints \$HOME" out_line "$TEST_HOME"

fresh
run_shell 'echo ~/dir1'
expect 3 "echo ~/dir1 expands the prefix" out_line "$TEST_HOME/dir1"

fresh
run_shell 'cd ~'
expect 3 "cd ~ moves to \$HOME (seen in the prompt)" raw_has ":$TEST_HOME> "

fresh
run_shell 'echo ~x'
expect 3 "~x is not expanded" out_line "~x"

# ---------------------------------------------------------------- Part 4
# $PATH search: commands without '/' are looked up in $PATH, or reported not found.
fresh
run_shell 'definitely_not_a_command_xyz'
expect 4 "unknown command reports 'not found'" err_has "not found"

fresh
run_shell 'ls /'
expect 4 "ls is found through \$PATH" out_line "(usr|bin|tmp)"

# ---------------------------------------------------------------- Part 5
# External commands: fork + execv, with all arguments passed through.
fresh
run_shell 'echo hello world'
expect 5 "echo with arguments" out_line "hello world"

fresh
run_shell '/bin/echo absolute path'
expect 5 "command given as an absolute path" out_line "absolute path"

fresh
touch "$WORK/.hidden_file"
run_shell 'ls -a'
expect 5 "ls -a (flags are passed)" out_line "\.hidden_file"

fresh
run_shell '/nonexistent/prog'
expect 5 "a bad path prints an error" err_nonempty

fresh
run_shell 'echo one\necho two'
expect 5 "shell keeps running after a command" out_line "two"

# ---------------------------------------------------------------- Part 6
# I/O redirection: "> file" creates/overwrites with -rw-------, "< file" must be an
# existing regular file, and both may appear in either order.
fresh
run_shell 'echo hi > out.txt'
expect 6 "cmd > file writes the file" file_is out.txt "hi"
expect 6 "output file is created -rw-------" perms_are out.txt "-rw-------"
expect 6 "redirected output is not printed" out_lacks "hi"

fresh
printf 'an old line that is longer\n' > "$WORK/out.txt"
run_shell 'echo new > out.txt'
expect 6 "cmd > file overwrites (not appends)" file_is out.txt "new"

fresh
printf 'a\nb\nc\n' > "$WORK/in.txt"
run_shell 'wc -l < in.txt'
expect 6 "cmd < file reads the file" out_line " *3"

fresh
run_shell 'cat < missing.txt'
expect 6 "missing input file is an error" err_nonempty

fresh
mkdir "$WORK/somedir"
run_shell 'cat < somedir'
expect 6 "input that is not a regular file is an error" err_nonempty

fresh
printf 'x\ny\n' > "$WORK/in.txt"
run_shell 'cat < in.txt > out.txt'
expect 6 "cmd < in > out" file_is out.txt "$(printf 'x\ny')"

fresh
printf 'x\ny\n' > "$WORK/in.txt"
run_shell 'cat > out.txt < in.txt'
expect 6 "cmd > out < in" file_is out.txt "$(printf 'x\ny')"

fresh
printf 'keep me\n' > "$WORK/in.txt"
run_shell 'cat < in.txt'
expect 6 "input file is not modified" file_is in.txt "keep me"

# ---------------------------------------------------------------- Part 7
# Piping: up to two pipes; each command's stdout feeds the next command's stdin.
fresh
run_shell 'echo a b c | wc -w'
expect 7 "cmd1 | cmd2" out_line " *3"

fresh
touch "$WORK/apple" "$WORK/banana" "$WORK/cherry"
run_shell 'ls | sort -r | head -n 1'
expect 7 "cmd1 | cmd2 | cmd3" out_line "cherry"

# ---------------------------------------------------------------- Part 8
# Background processing: "cmd &" prints "[N] PID" and returns to the prompt at once;
# completion prints "[N] + done CMD". The short foreground sleeps give the background
# job time to finish, and the next loop iteration reports it.
fresh
run_shell 'sleep 1 &\njobs\nsleep 2\necho after'
expect 8 "cmd & prints [N] PID" out_line "\[1\] [0-9]+"
expect 8 "jobs lists the running job" out_line "\[1\]\+ [0-9]+ sleep 1 &"
expect 8 "completion prints [N] + done CMD" out_has "done sleep 1 &"

fresh
run_shell 'sleep 1 &\nsleep 1 &\nsleep 2\necho x'
expect 8 "job numbers increase" out_line "\[2\] [0-9]+"

fresh
run_shell 'echo bg > out.txt &\nsleep 1\necho x'
expect 8 "background with output redirection" file_is out.txt "bg"

fresh
run_shell 'echo a b | wc -w &\nsleep 1\necho x'
expect 8 "background pipeline runs" out_line " *2"

# ---------------------------------------------------------------- Part 9
# Built-ins: cd (with the three required errors), jobs, and exit (waits for background
# jobs, then shows the last three valid commands).
fresh
mkdir "$WORK/sub"
run_shell 'cd sub'
expect 9 "cd DIR changes directory" raw_has ":$WORK/sub> "

fresh
mkdir "$WORK/sub"
run_shell 'cd sub\ncd'
expect 9 "cd with no arguments goes to \$HOME" raw_has ":$TEST_HOME> "

fresh
run_shell 'cd a b'
expect 9 "cd with two arguments is an error" err_nonempty

fresh
run_shell 'cd nowhere'
expect 9 "cd to a missing directory is an error" err_nonempty

fresh
touch "$WORK/file.txt"
run_shell 'cd file.txt'
expect 9 "cd to a file is an error" err_nonempty

fresh
run_shell 'jobs'
expect 9 "jobs with nothing running says so" out_line ".+"

fresh
run_shell ''
expect 9 "exit with no history says so" out_line ".+"

fresh
run_shell 'echo one\necho two\necho three\necho four'
expect 9 "exit shows the last three valid commands" out_has "echo four"
expect 9 "exit drops older commands" out_lacks "echo one"

fresh
run_shell 'sleep 2 &'
expect 9 "exit waits for background jobs" [ "$SECS" -ge 1 ]

# ---------------------------------------------------------------- misc
# Robustness: input the spec does not mention must still be handled quietly.
fresh
run_shell '\n\n   \n'
expect misc "blank lines do not crash or print errors" err_empty

echo "integration: $pass/$((pass + fail)) checks passed"
[ "$fail" -eq 0 ]
