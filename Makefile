CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
CPPFLAGS += -Iinclude

ifeq ($(OS),Windows_NT)
EXE = .exe
else
EXE =
endif

.PHONY: all test run clean
all: elevator_sim$(EXE)

elevator_sim$(EXE): src/main.c src/elevator.c src/hardware.c include/elevator.h include/hardware.h
	$(CC) $(CPPFLAGS) $(CFLAGS) src/main.c src/elevator.c src/hardware.c -o $@

test_elevator$(EXE): tests/test_elevator.c src/elevator.c src/hardware.c include/elevator.h include/hardware.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_elevator.c src/elevator.c src/hardware.c -o $@

test: test_elevator$(EXE)
	./test_elevator$(EXE)

run: elevator_sim$(EXE)
	./elevator_sim$(EXE)

clean:
	$(RM) elevator_sim$(EXE) test_elevator$(EXE)
