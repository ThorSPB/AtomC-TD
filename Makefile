CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99 -O2
SRCDIR  = src
BUILD   = build
TARGET  = $(BUILD)/atomc

SRCS := $(wildcard $(SRCDIR)/*.c)
OBJS := $(patsubst $(SRCDIR)/%.c,$(BUILD)/%.o,$(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILD)
	$(CC) $(CFLAGS) $(OBJS) -o $@

$(BUILD)/%.o: $(SRCDIR)/%.c $(SRCDIR)/atomc.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)

.PHONY: all clean
