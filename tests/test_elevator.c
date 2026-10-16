#include "elevator.h"
#include <assert.h>
#include <stdio.h>

static void test_init_and_invalid(void) {
    volatile uint8_t reg = 0xffu;
    Elevator e;
    assert(!elevator_init(NULL, &reg));
    assert(!elevator_init(&e, NULL));
    assert(elevator_init(&e, &reg));
    assert(e.state == IDLE && e.current_floor == 1u && reg == 0x10u);
    assert(elevator_request(&e, 6u) == REQUEST_INVALID);
    assert(elevator_request(NULL, 1u) == REQUEST_INVALID);
}

static void test_up_down_and_queue(void) {
    volatile uint8_t reg;
    Elevator e;
    assert(elevator_init(&e, &reg));
    assert(elevator_request(&e, 3u) == REQUEST_ACCEPTED);
    assert(elevator_request(&e, 3u) == REQUEST_DUPLICATE);
    elevator_tick(&e);
    assert(e.state == MOVING_UP && (reg & HW_MOTOR_UP));
    assert(elevator_request(&e, 3u) == REQUEST_DUPLICATE);
    assert(elevator_request(&e, 0u) == REQUEST_ACCEPTED);
    elevator_tick(&e);
    assert(e.current_floor == 2u && e.state == MOVING_UP);
    elevator_tick(&e);
    assert(e.current_floor == 3u && e.state == DOOR_OPEN);
    assert(!(reg & (HW_MOTOR_UP | HW_MOTOR_DOWN)) && (reg & HW_DOOR_OPEN));
    elevator_tick(&e);
    assert(e.state == DOOR_OPEN);
    elevator_tick(&e);
    assert(e.state == IDLE && !(reg & HW_DOOR_OPEN));
    elevator_tick(&e);
    assert(e.state == MOVING_DOWN && (reg & HW_MOTOR_DOWN));
    for (int i = 0; i < 3; ++i) elevator_tick(&e);
    assert(e.current_floor == 0u && e.state == DOOR_OPEN);
    assert(((reg & HW_FLOOR_MASK) >> HW_FLOOR_SHIFT) == 0u);
}

static void test_emergency_and_reset(void) {
    volatile uint8_t reg;
    Elevator e;
    assert(elevator_init(&e, &reg));
    assert(elevator_request(&e, 5u) == REQUEST_ACCEPTED);
    elevator_tick(&e);
    elevator_emergency_stop(&e);
    assert(e.state == EMERGENCY && e.count == 0u);
    assert(!(reg & (HW_MOTOR_UP | HW_MOTOR_DOWN | HW_DOOR_OPEN)));
    assert(reg & HW_EMERGENCY_LED);
    assert(elevator_request(&e, 2u) == REQUEST_EMERGENCY);
    elevator_tick(&e);
    assert(e.state == EMERGENCY);
    elevator_reset(&e);
    assert(e.state == IDLE && reg == 0x10u);
}

static void test_same_floor_and_capacity(void) {
    volatile uint8_t reg;
    Elevator e;
    assert(elevator_init(&e, &reg));
    assert(elevator_request(&e, 1u) == REQUEST_ACCEPTED);
    elevator_tick(&e);
    assert(e.state == DOOR_OPEN && (reg & HW_DOOR_OPEN));
    assert(elevator_request(&e, 1u) == REQUEST_DUPLICATE);
    elevator_reset(&e);
    for (unsigned floor = 0u; floor <= ELEVATOR_MAX_FLOOR; ++floor)
        assert(elevator_request(&e, floor) == REQUEST_ACCEPTED);
    assert(e.count == ELEVATOR_QUEUE_CAPACITY);
    assert(elevator_request(&e, 3u) == REQUEST_DUPLICATE);
}

int main(void) {
    test_init_and_invalid();
    test_up_down_and_queue();
    test_emergency_and_reset();
    test_same_floor_and_capacity();
    puts("All elevator tests passed.");
    return 0;
}
