#ifndef ELEVATOR_H
#define ELEVATOR_H

#include <stdbool.h>
#include <stdint.h>
#include "hardware.h"

#define ELEVATOR_MIN_FLOOR 0u
#define ELEVATOR_MAX_FLOOR 5u
#define ELEVATOR_DOOR_TICKS 2u
#define ELEVATOR_QUEUE_CAPACITY 6u

typedef enum {
    IDLE,
    MOVING_UP,
    MOVING_DOWN,
    DOOR_OPEN,
    EMERGENCY
} ElevatorState;

typedef enum {
    REQUEST_ACCEPTED,
    REQUEST_DUPLICATE,
    REQUEST_INVALID,
    REQUEST_EMERGENCY,
    REQUEST_FULL
} RequestResult;

typedef struct {
    uint8_t current_floor;
    uint8_t target_floor;
    uint8_t door_ticks_left;
    ElevatorState state;
    uint8_t queue[ELEVATOR_QUEUE_CAPACITY];
    uint8_t head;
    uint8_t count;
    Hardware hw;
} Elevator;

bool elevator_init(Elevator *e, volatile uint8_t *port);
RequestResult elevator_request(Elevator *e, unsigned floor);
void elevator_tick(Elevator *e);
void elevator_emergency_stop(Elevator *e);
void elevator_reset(Elevator *e);
bool elevator_is_busy(const Elevator *e);
const char *elevator_state_name(ElevatorState state);

#endif
