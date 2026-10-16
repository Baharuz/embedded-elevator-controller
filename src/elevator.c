#include "elevator.h"
#include <stddef.h>

static void open_at_target(Elevator *e) {
    hw_stop(&e->hw);
    hw_set_door(&e->hw, true);
    e->state = DOOR_OPEN;
    e->door_ticks_left = ELEVATOR_DOOR_TICKS;
}

static bool pop_request(Elevator *e, uint8_t *floor) {
    if (e->count == 0u) return false;
    *floor = e->queue[e->head];
    e->head = (uint8_t)((e->head + 1u) % ELEVATOR_QUEUE_CAPACITY);
    --e->count;
    return true;
}

bool elevator_init(Elevator *e, volatile uint8_t *port) {
    if (e == NULL || port == NULL) return false;
    e->current_floor = 1u;
    e->target_floor = 1u;
    e->door_ticks_left = 0u;
    e->state = IDLE;
    e->head = 0u;
    e->count = 0u;
    hw_init(&e->hw, port, e->current_floor);
    return true;
}

RequestResult elevator_request(Elevator *e, unsigned floor) {
    if (e == NULL || floor > ELEVATOR_MAX_FLOOR) return REQUEST_INVALID;
    if (e->state == EMERGENCY) return REQUEST_EMERGENCY;
    if ((e->state == MOVING_UP || e->state == MOVING_DOWN || e->state == DOOR_OPEN) &&
        floor == e->target_floor) return REQUEST_DUPLICATE;
    for (uint8_t i = 0u; i < e->count; ++i) {
        if (e->queue[(e->head + i) % ELEVATOR_QUEUE_CAPACITY] == floor)
            return REQUEST_DUPLICATE;
    }
    if (e->count == ELEVATOR_QUEUE_CAPACITY) return REQUEST_FULL;
    e->queue[(e->head + e->count) % ELEVATOR_QUEUE_CAPACITY] = (uint8_t)floor;
    ++e->count;
    return REQUEST_ACCEPTED;
}

void elevator_tick(Elevator *e) {
    if (e == NULL) return;
    switch (e->state) {
        case IDLE: {
            uint8_t next;
            if (!pop_request(e, &next)) break;
            e->target_floor = next;
            if (next > e->current_floor) {
                hw_move_up(&e->hw);
                e->state = MOVING_UP;
            } else if (next < e->current_floor) {
                hw_move_down(&e->hw);
                e->state = MOVING_DOWN;
            } else {
                open_at_target(e);
            }
            break;
        }
        case MOVING_UP:
            if (e->current_floor >= ELEVATOR_MAX_FLOOR) {
                elevator_emergency_stop(e);
                break;
            }
            ++e->current_floor;
            hw_set_floor(&e->hw, e->current_floor);
            if (e->current_floor == e->target_floor) open_at_target(e);
            break;
        case MOVING_DOWN:
            if (e->current_floor <= ELEVATOR_MIN_FLOOR) {
                elevator_emergency_stop(e);
                break;
            }
            --e->current_floor;
            hw_set_floor(&e->hw, e->current_floor);
            if (e->current_floor == e->target_floor) open_at_target(e);
            break;
        case DOOR_OPEN:
            if (e->door_ticks_left > 0u) --e->door_ticks_left;
            if (e->door_ticks_left == 0u) {
                hw_set_door(&e->hw, false);
                e->state = IDLE;
                e->target_floor = e->current_floor;
            }
            break;
        case EMERGENCY:
            hw_stop(&e->hw);
            break;
        default:
            elevator_emergency_stop(e);
            break;
    }
}

void elevator_emergency_stop(Elevator *e) {
    if (e == NULL) return;
    hw_stop(&e->hw);
    hw_set_door(&e->hw, false);
    hw_set_emergency(&e->hw, true);
    e->state = EMERGENCY;
    e->target_floor = e->current_floor;
    e->door_ticks_left = 0u;
    e->head = 0u;
    e->count = 0u;
}

void elevator_reset(Elevator *e) {
    if (e == NULL) return;
    hw_stop(&e->hw);
    hw_set_door(&e->hw, false);
    hw_set_emergency(&e->hw, false);
    hw_set_floor(&e->hw, e->current_floor);
    e->state = IDLE;
    e->target_floor = e->current_floor;
    e->door_ticks_left = 0u;
    e->head = 0u;
    e->count = 0u;
}

bool elevator_is_busy(const Elevator *e) {
    return e != NULL && (e->state != IDLE || e->count != 0u);
}

const char *elevator_state_name(ElevatorState state) {
    switch (state) {
        case IDLE: return "IDLE";
        case MOVING_UP: return "MOVING_UP";
        case MOVING_DOWN: return "MOVING_DOWN";
        case DOOR_OPEN: return "DOOR_OPEN";
        case EMERGENCY: return "EMERGENCY";
        default: return "UNKNOWN";
    }
}
