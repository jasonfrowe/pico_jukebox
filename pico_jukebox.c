#include <stdio.h>
#include "pico/stdlib.h"

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

// --- Main Application ---

int main() {
    stdio_init_all();
    setup_pins();

    printf("Pico OPL2 Jukebox Starting...\n");

    // 1. Reset FPGA
    gpio_put(PIN_RST, 0); sleep_ms(10);
    gpio_put(PIN_RST, 1); sleep_ms(10);

    // 2. Setup Instrument (Violin-ish / Sustaining)
    // Modulator
    opl_write(false, 0x20); opl_write(true, 0x01); // Multiple
    opl_write(false, 0x40); opl_write(true, 0x10); // Level (High output)
    opl_write(false, 0x60); opl_write(true, 0xF0); // Attack/Decay
    opl_write(false, 0x80); opl_write(true, 0x77); // Sustain/Release
    
    // Carrier
    opl_write(false, 0x23); opl_write(true, 0x01); 
    opl_write(false, 0x43); opl_write(true, 0x00); // Max Volume
    opl_write(false, 0x63); opl_write(true, 0xF0);
    opl_write(false, 0x83); opl_write(true, 0x77);
    
    // FM Mode
    opl_write(false, 0xC0); opl_write(true, 0x01); // Algorithm 0 (FM)

    // 3. Main Loop
    // We will pick a fixed "Block" (Octave) of 4.
    // Block 4 is bits 2-4: 100 -> 0x10
    // Key On is bit 5:     1     -> 0x20
    // Base value for B0 is 0x30.
    uint8_t base_b0 = 0x30; 

    while (true) {
        // Sweep frequency numbers (0 to 1023 max for 10 bits)
        // We sweep 200 to 800
        for (uint16_t f_num = 200; f_num < 800; f_num += 5) {
            
            // 1. Write Low 8 bits to 0xA0
            opl_write(false, 0xA0); 
            opl_write(true,  f_num & 0xFF);

            // 2. Write High 2 bits to 0xB0... 
            // BUT KEEP KEY-ON (0x20) AND BLOCK (0x10) SET!
            uint8_t high_bits = (f_num >> 8) & 0x03;
            
            opl_write(false, 0xB0);
            opl_write(true,  base_b0 | high_bits); 

            sleep_ms(10);
        }
        
        sleep_ms(500); // Pause at top
    }
} 