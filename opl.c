#include <stdio.h>
#include "pico/stdlib.h"
#include "opl.h"

// F-Numbers for Octave 4
const uint16_t fnum_table[12] = {
    344, 363, 385, 408, 432, 458, 485, 514, 544, 577, 611, 647
};

// Shadow registers for all 9 channels
// We need this to remember the Block/F-Number when we send a NoteOff
uint8_t shadow_b0[9] = {0}; 

uint16_t midi_to_opl_freq(uint8_t midi_note) {
    if (midi_note < 24) midi_note = 24;
    
    int block = (midi_note - 24) / 12;
    if (block > 7) block = 7;
    
    int note_idx = (midi_note - 24) % 12;
    uint16_t f_num = fnum_table[note_idx];

    uint8_t high_byte = 0x20 | (block << 2) | ((f_num >> 8) & 0x03);
    uint8_t low_byte = f_num & 0xFF;

    return (high_byte << 8) | low_byte;
}

void OPL_NoteOn(uint8_t channel, uint8_t midi_note) {
    if (channel > 8) return; // Safety

    // 1. Calculate params using the helper
    uint16_t freq_data = midi_to_opl_freq(midi_note);
    uint8_t high_byte = (freq_data >> 8) & 0xFF; // Includes 0x20 (KeyOn)
    uint8_t low_byte  = freq_data & 0xFF;

    // 2. Write to OPL
    opl_write(false, 0xA0 + channel);
    opl_write(true,  low_byte);
    
    opl_write(false, 0xB0 + channel);
    opl_write(true,  high_byte);

    // 3. Update Shadow (Exclude KeyOn bit for safe storage)
    shadow_b0[channel] = high_byte & ~0x20; 
}

void OPL_NoteOff(uint8_t channel) {
    if (channel > 8) return;

    // Retrieve the pitch for this channel, but keep KeyOn (0x20) CLEARED.
    uint8_t safe_release_byte = shadow_b0[channel];

    opl_write(false, 0xB0 + channel);
    opl_write(true,  safe_release_byte);
}

void opl_clear() {
    for (int i = 0; i < 256; i++) {
        opl_write(false, i);
        opl_write(true,  0x00);
    }
    // Clear shadow memory too
    for (int i=0; i<9; i++) shadow_b0[i] = 0;
}