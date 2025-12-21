#include <stdio.h>
#include "pico/stdlib.h"
#include "opl.h"

// ==========================================================
// OPL2 MUSIC THEORY LOGIC
// ==========================================================

// F-Numbers for Octave 4 (Block 4)
// These map to: C, C#, D, D#, E, F, F#, G, G#, A, A#, B
const uint16_t fnum_table[12] = {
    344, 363, 385, 408, 432, 458, 485, 514, 544, 577, 611, 647
};

// Map MIDI Note (0-127) to OPL2 Block & F-Number
// Returns a 16-bit value:
//   Bits 0-9:   F-Number (10 bits)
//   Bits 10-12: Block (3 bits)
//   Bit  13:    Key On (Always Set for convenience)
uint16_t midi_to_opl_freq(uint8_t midi_note) {
    // 1. Calculate the "Block" (Octave)
    // MIDI Note 60 is Middle C (C4).
    // In OPL2, Block 4 is roughly the C4 range.
    // We start MIDI note 24 (C1) at Block 0.
    
    int block;
    int note_idx;

    // Handle extremely low notes (clamp to Block 0)
    if (midi_note < 24) {
        block = 0;
        note_idx = midi_note % 12;
    } else {
        block = (midi_note - 24) / 12;
        note_idx = (midi_note - 24) % 12;
    }

    // Clamp block to max 7
    if (block > 7) block = 7;

    // 2. Get the F-Number for this note name
    uint16_t f_num = fnum_table[note_idx];

    // 3. Pack the data
    // Format: [KeyOn][Block 3-bits][F-Num High 2-bits] [F-Num Low 8-bits]
    // Return combined 16-bit generic value for easier handling:
    // [0 0 1 B2 B1 B0 F9 F8] [F7 ... F0]
    
    // Construct the "High Byte" (Register B0) part:
    // KeyOn (0x20) | Block (block << 2) | Top 2 bits of FNum (f_num >> 8)
    uint8_t high_byte = 0x20 | (block << 2) | ((f_num >> 8) & 0x03);
    
    // Construct the "Low Byte" (Register A0) part:
    uint8_t low_byte = f_num & 0xFF;

    return (high_byte << 8) | low_byte;
}

// Add this global variable (outside main)
uint8_t shadow_b0 = 0;  // Remembers last B0 value without key-on bit

void OPL_NoteOn(uint8_t channel, uint8_t midi_note) {
    if (channel != 0) return;  // Only channel 0 for now

    if (midi_note < 24) midi_note = 24;
    uint8_t block = (midi_note - 24) / 12;
    if (block > 7) block = 7;
    uint8_t note_idx = (midi_note - 24) % 12;
    uint16_t f_num = fnum_table[note_idx];

    uint8_t low_byte = f_num & 0xFF;
    uint8_t high_byte = 0x20 | (block << 2) | ((f_num >> 8) & 0x03);  // Key-on = 1

    opl_write(false, 0xA0 + channel);
    opl_write(true, low_byte);
    opl_write(false, 0xB0 + channel);
    opl_write(true, high_byte);

    // Save the frequency/block part for clean note-off (exclude key-on bit)
    shadow_b0 = high_byte & ~0x20;
}

void OPL_NoteOff(uint8_t channel) {
    if (channel != 0) return;

    // Write the preserved frequency/block, with key-on = 0
    // This keeps the pitch correct during release
    printf("NoteOff: Writing B0 = 0x%02X\n", shadow_b0);
    opl_write(false, 0xB0 + channel);
    opl_write(true, shadow_b0);
}

// Wipe all OPL2 registers to 0x00 to ensure a clean state
void opl_clear() {
    // The OPL2 has 256 registers (0x00 - 0xFF)
    for (int i = 0; i < 256; i++) {
        opl_write(false, i);    // Write Address
        opl_write(true,  0x00); // Write Data 0x00
    }
}