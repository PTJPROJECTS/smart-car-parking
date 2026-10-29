# Host build: unit tests and the command line simulator.
# The Arduino firmware is built with PlatformIO, see README.

CC      ?= gcc
CFLAGS  ?= -std=c99 -Wall -Wextra -Werror -O1 -g
CFLAGS  += -Ilib/parking_core

BUILD   := build
CORE    := lib/parking_core/parking.c

.PHONY: all test pytest clean

all: $(BUILD)/test_parking

$(BUILD):
	mkdir -p $@

$(BUILD)/test_parking: host_tests/test_parking.c $(CORE) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

test: all
	./$(BUILD)/test_parking

clean:
	rm -rf $(BUILD)
