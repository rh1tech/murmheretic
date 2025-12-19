// PS/2 Mouse Wrapper for DOOM
// Interfaces ps2mouse driver with DOOM's event system
// SPDX-License-Identifier: GPL-2.0-or-later

// Include d_event.h first - it pulls in doomtype.h which defines its own boolean type
// This must come before ps2mouse.h which uses stdbool.h
#include "d_event.h"
#include "ps2mouse_wrapper.h"
#include "ps2mouse.h"
#include <stdint.h>

// External DOOM function to post events
extern void D_PostEvent(event_t *ev);

// Mouse sensitivity multiplier (increase for faster response)
#define MOUSE_SENSITIVITY_MULT 2

// Maximum delta per tick to prevent abrupt jumps
#define MOUSE_MAX_DELTA 40

// Track previous button state to detect changes
static uint8_t prev_buttons = 0;

// Clamp value to range
static inline int16_t clamp_delta(int16_t val, int16_t max_val) {
    if (val > max_val) return max_val;
    if (val < -max_val) return -max_val;
    return val;
}

void ps2mouse_wrapper_init(void) {
    ps2mouse_init();
    prev_buttons = 0;
}

void ps2mouse_wrapper_tick(void) {
    int16_t dx, dy;
    int8_t wheel;
    uint8_t buttons;
    
    // Get accumulated mouse movement
    int has_motion = ps2mouse_get_state(&dx, &dy, &wheel, &buttons);
    
    // Only post event if there's actual motion or button change
    // DOOM expects: data1 = buttons, data2 = X motion (turn), data3 = Y motion (forward)
    if (has_motion || buttons != prev_buttons) {
        event_t ev;
        ev.type = ev_mouse;
        
        // PS/2 button mapping matches DOOM: bit 0 = left, bit 1 = right, bit 2 = middle
        // Only include button state, mask off any other bits
        ev.data1 = buttons & 0x07;
        
        // Clamp deltas to prevent abrupt movements from noise or fast flicks
        dx = clamp_delta(dx, MOUSE_MAX_DELTA);
        dy = clamp_delta(dy, MOUSE_MAX_DELTA);
        
        // X motion = turn (positive = turn right)
        // PS/2 X is positive when moving right
        // Apply sensitivity multiplier
        ev.data2 = dx * MOUSE_SENSITIVITY_MULT;
        
        // Y motion = forward/backward
        // PS/2 Y is positive when moving UP (away from user)
        // DOOM expects positive = forward, negative = backward
        // Apply sensitivity multiplier
        ev.data3 = dy * MOUSE_SENSITIVITY_MULT;
        
        ev.data4 = 0;
        
        D_PostEvent(&ev);
        
        prev_buttons = buttons & 0x07;
    }
}
