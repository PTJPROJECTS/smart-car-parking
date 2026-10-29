# Host build: unit tests and the command line simulator.
# The Arduino firmware is built with PlatformIO, see README.

CC      ?= gcc
CFLAGS  ?= -std=c99 -Wall -Wextra -Werror -O1 -g
CFLAGS  += -Ilib/parking_core

BUILD   := build
CORE    := lib/parking_core/parking.c

.PHONY: all test pytest sim clean

all: $(BUILD)/test_parking $(BUILD)/parking_sim

$(BUILD):
	mkdir -p $@

$(BUILD)/test_parking: host_tests/test_parking.c $(CORE) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/parking_sim: host/parking_sim.c $(CORE) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

test: all
	./$(BUILD)/test_parking

pytest: all
	python3 -m pytest -q tests

sim: all
	./$(BUILD)/parking_sim -t < scenarios/basic.txt

clean:
	rm -rf $(BUILD)
