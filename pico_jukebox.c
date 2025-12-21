#include <stdio.h>
#include "pico/stdlib.h"
#include "opl.h"

// --- Hardware Wiring Configuration ---
// Data Bus: GP0 - GP7
#define PIN_D0  0 
// Control Lines
#define PIN_A0  8   // 0 = Index, 1 = Data
#define PIN_WE  9   // Write Enable (Active Low)
#define PIN_CS  10  // Chip Select (Active Low)
#define PIN_RST 11  // Reset (Active Low)

// Mask for all data pins (GPIO 0-7)
#define DATA_MASK 0xFF

// --- Low Level Bus Interface ---

// Write a byte to the OPL2/FPGA
void opl_write(bool is_data, uint8_t data) {
    // 1. Set Address Line (0 for Index Register, 1 for Data)
    gpio_put(PIN_A0, is_data);

    // 2. Put Data on the Bus (GPIO 0-7)
    // This sets all 8 pins at once. 
    // We mask with DATA_MASK so we don't touch other pins.
    gpio_put_masked(DATA_MASK, data);

    // 3. Chip Select LOW (Select the chip)
    gpio_put(PIN_CS, 0);

    // 4. Write Enable LOW (Pulse to trigger write)
    gpio_put(PIN_WE, 0);
    sleep_us(1); // Short delay to ensure FPGA sees the LOW state
    gpio_put(PIN_WE, 1); // Return to HIGH

    // 5. Chip Select HIGH (Deselect)
    gpio_put(PIN_CS, 1);
    
    // 6. Mandatory OPL2 Delays
    // The real OPL2 chip is slow. 
    // After writing an Address (Index), it needs ~3.3us.
    // After writing Data, it needs ~23us.
    // We wait 25us to be safe for both cases.
    sleep_us(25); 
}

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

int main() {
    stdio_init_all();
    setup_pins();

    // 1. Hardware Reset (Pulse Low)
    // This resets the JTOPL Verilog core
    gpio_put(PIN_RST, 0); 
    sleep_ms(10);
    gpio_put(PIN_RST, 1); 
    sleep_ms(10);

    // 2. Software Initialization (The Missing Step!)
    printf("Initializing OPL2...\n");
    
    // A. Wipe registers
    opl_clear();
    
    // B. Enable Waveform Select (Crucial Standard Init)
    // Register 0x01, Bit 5 (0x20) = 1
    // Without this, some OPL2 cores behave unpredictably.
    opl_write(false, 0x01); 
    opl_write(true,  0x20);

    // C. Set Note Select / CSM to normal
    // Register 0x08 = 0x00 (Split Point = 0, Keyboard Split = 0)
    opl_write(false, 0x08);
    opl_write(true,  0x00);

    // 3. Setup Instrument (Violin-ish / Sustaining)
    // Modulator
    opl_write(false, 0x20); opl_write(true, 0x01); // Multiple
    opl_write(false, 0x40); opl_write(true, 0x10); // Level (High output)
    opl_write(false, 0x60); opl_write(true, 0xF0); // Attack/Decay
    opl_write(false, 0x80); opl_write(true, 0x00); // 7C); // Medium Sustain, Fast Release (Snappier end)
    opl_write(false, 0xE0); opl_write(true, 0x00); // No Vibrato, No Tremolo, No Key Scale, No Waveform Select

    // Carrier
    opl_write(false, 0x23); opl_write(true, 0x01); 
    opl_write(false, 0x43); opl_write(true, 0x00); // Max Volume
    opl_write(false, 0x63); opl_write(true, 0xF0);
    opl_write(false, 0x83); opl_write(true, 0x00); // 7C);
    opl_write(false, 0xE3); opl_write(true, 0x00);
    
    // FM Mode
    opl_write(false, 0xC0); opl_write(true, 0x01); // Algorithm 0 (FM)
    
    uint8_t scale[] = {60, 62, 64, 65, 67, 69, 71, 72}; // C major scale

    while(true) {
        for(int i=0; i<8; i++) {
            OPL_NoteOn(0, scale[i]);
            sleep_ms(400);  // Longer hold = fuller notes
        }
        // Optional short pause
        OPL_NoteOff(0);
        sleep_ms(800);
    }
}