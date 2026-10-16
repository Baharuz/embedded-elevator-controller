#define _POSIX_C_SOURCE 200809L
#include "elevator.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
static unsigned long long now_ms(void) { return (unsigned long long)GetTickCount64(); }
static int poll_line(char *line, size_t size) {
    static size_t used = 0u;
    while (_kbhit()) {
        int ch = _getch();
        if (ch == '\r' || ch == '\n') {
            line[used] = '\0';
            putchar('\n');
            used = 0u;
            return 1;
        }
        if (ch == '\b' && used > 0u) {
            --used;
            fputs("\b \b", stdout);
        } else if (isprint((unsigned char)ch) && used + 1u < size) {
            line[used++] = (char)ch;
            putchar(ch);
        }
    }
    Sleep(25);
    return 0;
}
#else
#include <sys/select.h>
#include <unistd.h>
static unsigned long long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long long)ts.tv_sec * 1000ull + (unsigned long long)ts.tv_nsec / 1000000ull;
}
static int poll_line(char *line, size_t size) {
    fd_set input;
    struct timeval timeout = {0, 25000};
    FD_ZERO(&input);
    FD_SET(STDIN_FILENO, &input);
    int ready = select(STDIN_FILENO + 1, &input, NULL, NULL, &timeout);
    if (ready < 0) return errno == EINTR ? 0 : -1;
    if (ready == 0) return 0;
    if (fgets(line, (int)size, stdin) == NULL) return -1;
    return 1;
}
#endif

#define TICK_MS 600ull

static void print_status(const Elevator *e) {
    uint8_t reg = hw_read(&e->hw);
    printf("\nFloor %u | Target %u | %-11s | Queue:", e->current_floor,
           e->target_floor, elevator_state_name(e->state));
    if (e->count == 0u) printf(" empty");
    for (uint8_t i = 0u; i < e->count; ++i)
        printf(" %u", e->queue[(e->head + i) % ELEVATOR_QUEUE_CAPACITY]);
    printf("\nPORT 0x%02X | UP %s | DOWN %s | DOOR %s | EMERGENCY %s\n",
           (unsigned)reg, (reg & HW_MOTOR_UP) ? "ON" : "OFF",
           (reg & HW_MOTOR_DOWN) ? "ON" : "OFF",
           (reg & HW_DOOR_OPEN) ? "OPEN" : "CLOSED",
           (reg & HW_EMERGENCY_LED) ? "ON" : "OFF");
    fflush(stdout);
}

static void print_register(const Elevator *e) {
    uint8_t reg = hw_read(&e->hw);
    printf("PORT = ");
    for (int bit = 7; bit >= 0; --bit) putchar((reg & (1u << bit)) ? '1' : '0');
    printf(" (0x%02X)\nBits 7..4: floor; 3: emergency LED; 2: door; 1: down; 0: up\n",
           (unsigned)reg);
}

static void handle_line(Elevator *e, char *line, bool *running) {
    char *end;
    while (isspace((unsigned char)*line)) ++line;
    if (*line == '\0') return;
    if (*line >= '0' && *line <= '9') {
        errno = 0;
        unsigned long floor = strtoul(line, &end, 10);
        while (isspace((unsigned char)*end)) ++end;
        if (errno != 0 || *end != '\0' || floor > ELEVATOR_MAX_FLOOR) {
            printf("Invalid floor. Use %u..%u.\n", ELEVATOR_MIN_FLOOR, ELEVATOR_MAX_FLOOR);
            return;
        }
        RequestResult result = elevator_request(e, (unsigned)floor);
        switch (result) {
            case REQUEST_ACCEPTED: printf("Floor %lu queued.\n", floor); break;
            case REQUEST_DUPLICATE: printf("Floor %lu already requested.\n", floor); break;
            case REQUEST_EMERGENCY: puts("Request rejected: reset after emergency."); break;
            case REQUEST_FULL: puts("Request rejected: queue full."); break;
            default: puts("Invalid floor."); break;
        }
    } else if (line[1] == '\0') {
        switch (tolower((unsigned char)line[0])) {
            case 'e': elevator_emergency_stop(e); puts("EMERGENCY STOP"); break;
            case 'r': elevator_reset(e); puts("System reset."); break;
            case 'v': print_register(e); break;
            case 'q': *running = false; break;
            case 'h': puts("Enter a floor 0..5, e = emergency, r = reset, v = register, q = quit."); break;
            default: puts("Unknown command. Enter h for help."); break;
        }
    } else {
        puts("Unknown command. Enter h for help.");
    }
    print_status(e);
}

int main(void) {
    volatile uint8_t port = 0u;
    Elevator e;
    char line[128];
    if (!elevator_init(&e, &port)) return EXIT_FAILURE;
    puts("Elevator simulator: enter floor 0..5, e emergency, r reset, v register, q quit.");
    puts("Each floor and door step takes 600 ms. Commands remain available while moving.");
    print_status(&e);
    unsigned long long next_tick = now_ms() + TICK_MS;
    bool running = true;
    while (running) {
        int input = poll_line(line, sizeof line);
        if (input < 0) break;
        if (input > 0) {
            line[strcspn(line, "\r\n")] = '\0';
            handle_line(&e, line, &running);
        }
        unsigned long long now = now_ms();
        if (now >= next_tick) {
            if (elevator_is_busy(&e) && e.state != EMERGENCY) {
                elevator_tick(&e);
                print_status(&e);
            }
            next_tick = now + TICK_MS;
        }
    }
    return EXIT_SUCCESS;
}
