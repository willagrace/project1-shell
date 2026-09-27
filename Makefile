SRC := src
OBJ := obj
BIN := bin
TEST := tests
EXECUTABLE:= shell

SRCS := $(wildcard $(SRC)/*.c)
OBJS := $(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(SRCS))
INCS := -Iinclude/
DIRS := $(OBJ)/ $(BIN)/
EXEC := $(BIN)/$(EXECUTABLE)

# unit tests link every object except the one containing the shell's main()
TEST_SRCS := $(wildcard $(TEST)/test_*.c)
LIB_OBJS := $(filter-out $(OBJ)/main.o,$(OBJS))

CC := gcc
CFLAGS := -g -Wall -std=c99 $(INCS)
LDFLAGS :=

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(EXEC)

$(OBJ)/%.o: $(SRC)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(EXEC)
	$(EXEC)

# build and run each unit test separately so one failing build doesn't block the others,
# then run the end-to-end tests against bin/shell
test: $(EXEC) $(LIB_OBJS)
	@status=0; \
	for src in $(TEST_SRCS); do \
		name=$$(basename $$src .c); \
		echo "== $$name"; \
		$(CC) $(CFLAGS) $$src $(LIB_OBJS) -o $(BIN)/$$name && ./$(BIN)/$$name || status=1; \
	done; \
	echo "== integration"; \
	bash $(TEST)/integration.sh $(EXEC) || status=1; \
	exit $$status

clean:
	rm -rf $(OBJ)/*.o $(EXEC) $(BIN)/test_*

$(shell mkdir -p $(DIRS))

.PHONY: run clean all test

