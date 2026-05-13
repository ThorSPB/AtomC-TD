# Force bash so the `test` recipe works from any parent shell (PowerShell, cmd,
# git-bash). On Windows we must point at Git-for-Windows bash explicitly,
# because a plain `bash` resolves to C:\Windows\System32\bash.exe — the WSL
# launcher, which is not what we want here.
ifeq ($(OS),Windows_NT)
    SHELL := C:/Program Files/Git/bin/bash.exe
    EXE   := .exe
    # cc1.exe (the compiler proper that gcc spawns) silently fails to load
    # its DLLs unless mingw's bin directory is on PATH. If gcc is installed
    # at the standard chocolatey location, prepend it automatically so the
    # Makefile works from any parent shell (PowerShell, cmd, git-bash).
    ifneq ($(wildcard C:/ProgramData/mingw64/mingw64/bin/gcc.exe),)
        export PATH := C:/ProgramData/mingw64/mingw64/bin;$(PATH)
    endif
else
    SHELL := /bin/bash
    EXE   :=
endif

CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99 -O2
SRCDIR  = src
BUILD   = build
# On Windows gcc produces build/atomc.exe — the $(EXE) suffix keeps make
# tracking the real filename, otherwise it would re-link on every invocation.
TARGET  = $(BUILD)/atomc$(EXE)

SRCS := $(wildcard $(SRCDIR)/*.c)
OBJS := $(patsubst $(SRCDIR)/%.c,$(BUILD)/%.o,$(SRCS))

# Default input for `make run` — override with: make run FILE=tests/3.c
FILE ?= tests/0.c

all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILD)
	$(CC) $(CFLAGS) $(OBJS) -o $@

$(BUILD)/%.o: $(SRCDIR)/%.c $(SRCDIR)/atomc.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# The trailing ` ||:` is a shell-metachar trick. Without it, GNU Make on
# Windows decides the recipe is "simple" and runs it via CreateProcess instead
# of SHELL, which fails because rm/mkdir aren't native Windows commands. Any
# shell metacharacter (;, &&, ||) forces Make to hand the line to SHELL (bash)
# where rm/mkdir are always available.
$(BUILD):
	@mkdir -p $(BUILD) ||:

run: $(TARGET)
	@./$(TARGET) $(FILE) ||:

# Parse one file. Override with: make parse FILE=tests/parser/ok/04_struct.c
parse: $(TARGET)
	@./$(TARGET) --parse $(FILE) ||:

# Run every file in tests/parser/{ok,err} and check the expected outcome.
parse-test: $(TARGET)
	@pass=0; fail=0; \
	printf '%-44s  %s\n' "FILE" "RESULT"; \
	printf '%-44s  %s\n' "--------------------------------------------" "------"; \
	for f in tests/parser/ok/*.c; do \
		out=$$(./$(TARGET) --parse $$f 2>&1); \
		if echo "$$out" | grep -q "syntax OK"; then \
			printf '%-44s  \033[32m%s\033[0m\n' "$$f" "PASS"; \
			pass=$$((pass+1)); \
		else \
			printf '%-44s  \033[31m%s\033[0m  %s\n' "$$f" "FAIL" "$$out"; \
			fail=$$((fail+1)); \
		fi; \
	done; \
	for f in tests/parser/err/*.c; do \
		out=$$(./$(TARGET) --parse $$f 2>&1); \
		if echo "$$out" | grep -q "syntax error"; then \
			msg=$$(echo "$$out" | head -n1); \
			printf '%-44s  \033[32m%s\033[0m  %s\n' "$$f" "ERR! " "$$msg"; \
			pass=$$((pass+1)); \
		else \
			printf '%-44s  \033[31m%s\033[0m  (expected error, got: %s)\n' "$$f" "FAIL" "$$out"; \
			fail=$$((fail+1)); \
		fi; \
	done; \
	echo "---------------------------------------------------"; \
	printf '%d passed, %d failed\n' $$pass $$fail; \
	test $$fail -eq 0

test: $(TARGET)
	@fail=0; pass=0; total_tokens=0; \
	printf '%-16s  %-6s  %6s  %6s  %5s\n' "FILE" "RESULT" "LINES" "TOKENS" "KINDS"; \
	printf '%-16s  %-6s  %6s  %6s  %5s\n' "----------------" "------" "------" "------" "-----"; \
	for f in tests/*.c; do \
		base=$$(basename $$f .c); \
		expected=tests/expected/$$base.txt; \
		actual=$(BUILD)/$$base.out; \
		./$(TARGET) $$f > $$actual 2>&1; \
		if diff -q --strip-trailing-cr $$expected $$actual > /dev/null 2>&1; then \
			nlines=$$(wc -l < $$f | tr -d ' '); \
			ntok=$$(wc -l < $$actual | tr -d ' '); \
			nkind=$$(awk '{print $$2}' $$actual | sed 's/:.*//' | sort -u | wc -l | tr -d ' '); \
			printf '%-16s  \033[32m%-6s\033[0m  %6s  %6s  %5s\n' "$$f" "PASS" "$$nlines" "$$ntok" "$$nkind"; \
			pass=$$((pass+1)); \
			total_tokens=$$((total_tokens+ntok)); \
		else \
			printf '%-16s  \033[31m%-6s\033[0m\n' "$$f" "FAIL"; \
			diff -u --strip-trailing-cr $$expected $$actual || true; \
			fail=$$((fail+1)); \
		fi; \
	done; \
	echo "---------------------------------------------------"; \
	printf '%d passed, %d failed  --  %d tokens lexed in total\n' $$pass $$fail $$total_tokens; \
	echo "(see tests/README.md for what each file exercises)"; \
	test $$fail -eq 0

# Run the full test suite — lexer snapshots + parser ok/err. Used by CI.
check: test parse-test

clean:
	@rm -rf $(BUILD) ||:

.PHONY: all run parse test parse-test check clean
