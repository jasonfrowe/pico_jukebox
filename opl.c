#include <stdio.h>
#include "pico/stdlib.h"
#include "opl.h"

// ==========================================================
// OPL2 FREQUENCY MATH (TUNED FOR 4.0 MHz FPGA CLOCK)
// ==========================================================
// The real OPL2 runs at ~3.58 MHz. Our FPGA runs at 4.0 MHz.
// This makes the pitch ~12% too high (2 semitones).
// We compensate by scaling the F-Numbers down by (3.58 / 4.0 = 0.895).

// Original (3.58MHz): { 344, 363, 385, 408, 432, 458, 485, 514, 544, 577, 611, 647 }
// Tuned    (4.00MHz): { 308, 325, 345, 365, 387, 410, 434, 460, 487, 516, 547, 579 }

const uint16_t fnum_table[12] = {
    308, 325, 345, 365, 387, 410, 434, 460, 487, 516, 547, 579
};

// Shadow registers for all 9 channels
// We need this to remember the Block/F-Number when we send a NoteOff
uint8_t shadow_b0[9] = {0}; 

// Configure GPIO directions and initial states
void setup_pins() {
    // Initialize Data Bus (GP0 - GP7)
    for(int i=0; i<8; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_OUT);
    }

    // Initialize Control Lines
    int controls[] = {PIN_A0, PIN_WE, PIN_CS, PIN_RST};
    for(int i=0; i<4; i++) {
        gpio_init(controls[i]);
        gpio_set_dir(controls[i], GPIO_OUT);
        gpio_put(controls[i], 1); // Default HIGH (Inactive)
    }
}

uint16_t midi_to_opl_freq(uint8_t midi_note) {
    // 1. Handle Octave Offset
    // We previously used 24 (C1). To shift up an octave to hit Block 4 for Middle C,
    // we change the offset to 12 (C0).
    
    // Clamp to lowest valid note (C0) to prevent negative math
    if (midi_note < 12) midi_note = 12;
    
    int block = (midi_note - 12) / 12; // Adjusted offset
    int note_idx = (midi_note - 12) % 12;
    
    // OPL2 only supports Blocks 0-7. 
    // If the MIDI note is too high, we clamp to Block 7 
    // (and it will just play the highest pitch possible).
    if (block > 7) block = 7;

    uint16_t f_num = fnum_table[note_idx];

    // Pack: KeyOn (0x20) | Block | F-Num High
    uint8_t high_byte = 0x20 | (block << 2) | ((f_num >> 8) & 0x03);
    uint8_t low_byte = f_num & 0xFF;

    return (high_byte << 8) | low_byte;
}

void opl_write(bool is_data, uint8_t data) {
    gpio_put(PIN_A0, is_data);
    gpio_put_masked(DATA_MASK, data);
    gpio_put(PIN_CS, 0);
    gpio_put(PIN_WE, 0);
    sleep_us(1); 
    gpio_put(PIN_WE, 1);
    gpio_put(PIN_CS, 1);
    sleep_us(25); 
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