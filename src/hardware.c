#include "hardware.h"

void hw_init(Hardware *hw, volatile uint8_t *port, uint8_t floor) {
    hw->port = port;
    *port = 0u;
    hw_set_floor(hw, floor);
}

void hw_stop(Hardware *hw) {
    *hw->port &= 0xfcu;
}

void hw_move_up(Hardware *hw) {
    hw_stop(hw);
    hw_set_door(hw, false);
    *hw->port |= HW_MOTOR_UP;
}

void hw_move_down(Hardware *hw) {
    hw_stop(hw);
    hw_set_door(hw, false);
    *hw->port |= HW_MOTOR_DOWN;
}

void hw_set_door(Hardware *hw, bool open) {
    if (open) *hw->port |= HW_DOOR_OPEN;
    else *hw->port &= 0xfbu;
}

void hw_set_emergency(Hardware *hw, bool active) {
    if (active) *hw->port |= HW_EMERGENCY_LED;
    else *hw->port &= 0xf7u;
}

void hw_set_floor(Hardware *hw, uint8_t floor) {
    *hw->port = (uint8_t)((*hw->port & 0x0fu) |
                          ((floor & 0x0fu) << HW_FLOOR_SHIFT));
}

uint8_t hw_read(const Hardware *hw) {
    return *hw->port;
}
