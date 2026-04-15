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
	@fail=0; pass=0; \
	for f in tests/*.c; do \
		base=$$(basename $$f .c); \
		expected=tests/expected/$$base.txt; \
		actual=$(BUILD)/$$base.out; \
		./$(TARGET) $$f > $$actual 2>&1; \
		if diff -q --strip-trailing-cr $$expected $$actual > /dev/null 2>&1; then \
			echo "PASS $$f"; \
			pass=$$((pass+1)); \
		else \
			echo "FAIL $$f"; \
			diff -u --strip-trailing-cr $$expected $$actual || true; \
			fail=$$((fail+1)); \
		fi; \
	done; \
	echo "----"; \
	echo "$$pass passed, $$fail failed"; \
	test $$fail -eq 0

clean:
	rm -rf $(BUILD)

.PHONY: all run test clean
