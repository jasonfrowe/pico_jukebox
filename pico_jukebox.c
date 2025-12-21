#include <stdio.h>
#include "pico/stdlib.h"
#include "opl.h"
#include "instruments.h"
#include "song_data.h"

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

// --- Song Playback Logic ---
void play_song(const SongEvent* song) {
    int i = 0;
    while (true) {
        SongEvent event = song[i];
        
        if (event.delay_ms > 0) sleep_ms(event.delay_ms);

        // --- COMMAND HANDLER ---
        switch (event.type) {
            case 0: // Note Off
                OPL_NoteOff(event.channel);
                break;

            case 1: // Note On
                if (event.channel == 8) {
                    // Drums are special: Map Note Number -> Patch
                    load_drum_patch(8, event.note);
                }
                OPL_NoteOn(event.channel, event.note);
                break;

            case 2: // End of Song
                return; // Break the function (or loop)

            case 3: // Program Change (New!)
                // The 'note' field holds the Program Number (0-127)
                // We use our Library function to load the patch
                load_gm_instrument(event.channel, event.note);
                break;
        }

        i++;
    }
}

int main() {
    stdio_init_all();
    setup_pins();
    
    sleep_ms(3000); // Wait for FPGA
    
    // Init
    gpio_put(PIN_RST, 0); sleep_ms(10);
    gpio_put(PIN_RST, 1); sleep_ms(10);
    opl_clear();
    opl_write(false, 0x01); opl_write(true, 0x20); // Enable Waveforms

    printf("Jukebox Ready.\n");

    // 1. Set Defaults (Optional but good practice)
    // Initialize everyone to Piano just in case the MIDI file assumes defaults
    for(int i=0; i<9; i++) load_gm_instrument(i, 0);

    // 2. Play!
    // No more manual patch loading! The song data handles it.
    printf("Playing Song...\n");
    
    while(true) {
        play_song(midi_song);
        sleep_ms(2000);
    }
}