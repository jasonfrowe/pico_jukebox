#ifndef OPL_H
#define OPL_H

#include <stdint.h>
#include <stdbool.h>

// --- HARDWARE CONFIGURATION ---
// 1. FPGA Interface (Internal OPL Bus)
#define PIN_D0  0   // GP0-GP7 are Data
#define PIN_A0  8   // Address
#define PIN_WE  9   // Write Enable
#define PIN_CS  10  // Chip Select
#define PIN_RST 11  // FPGA Reset (The missing definition!)

// Mask for all data pins (GPIO 0-7)
#define DATA_MASK 0xFF

// --- Hardware & Driver Prototypes ---
extern void opl_write(bool is_data, uint8_t data);
extern void setup_pins();
extern void OPL_NoteOn(uint8_t channel, uint8_t midi_note);
extern void OPL_NoteOff(uint8_t channel);
extern uint16_t midi_to_opl_freq(uint8_t midi_note);
extern void opl_clear();

// Shadow variables used by opl.c
extern uint8_t shadow_b0[9];

// Note: SongEvent is now defined in queue.h to prevent circular dependencies

#endif // OPL_H