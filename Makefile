# Force bash so the `test` recipe works from any parent shell (PowerShell, cmd,
# git-bash). On Windows we must point at Git-for-Windows bash explicitly,
# because a plain `bash` resolves to C:\Windows\System32\bash.exe — the WSL
# launcher, which is not what we want here.
ifeq ($(OS),Windows_NT)
    SHELL := C:/Program Files/Git/bin/bash.exe
else
    SHELL := /bin/bash
endif

CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99 -O2
SRCDIR  = src
BUILD   = build
TARGET  = $(BUILD)/atomc

SRCS := $(wildcard $(SRCDIR)/*.c)
OBJS := $(patsubst $(SRCDIR)/%.c,$(BUILD)/%.o,$(SRCS))

# Default input for `make run` — override with: make run FILE=tests/3.c
FILE ?= tests/0.c

all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILD)
	$(CC) $(CFLAGS) $(OBJS) -o $@

$(BUILD)/%.o: $(SRCDIR)/%.c $(SRCDIR)/atomc.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

run: $(TARGET)
	./$(TARGET) $(FILE)

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

clean:
	rm -rf $(BUILD)

.PHONY: all run test clean
