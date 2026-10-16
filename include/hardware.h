#ifndef HARDWARE_H
#define HARDWARE_H

#include <stdbool.h>
#include <stdint.h>

#define HW_MOTOR_UP      (1u << 0)
#define HW_MOTOR_DOWN    (1u << 1)
#define HW_DOOR_OPEN     (1u << 2)
#define HW_EMERGENCY_LED (1u << 3)
#define HW_FLOOR_SHIFT   4u
#define HW_FLOOR_MASK    0xf0u

typedef struct {
    volatile uint8_t *port;
} Hardware;

void hw_init(Hardware *hw, volatile uint8_t *port, uint8_t floor);
void hw_stop(Hardware *hw);
void hw_move_up(Hardware *hw);
void hw_move_down(Hardware *hw);
void hw_set_door(Hardware *hw, bool open);
void hw_set_emergency(Hardware *hw, bool active);
void hw_set_floor(Hardware *hw, uint8_t floor);
uint8_t hw_read(const Hardware *hw);

#endif
