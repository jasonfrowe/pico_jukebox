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



// Add this helper function
void handle_drum_note(uint8_t note) {
    // Channel 8 is our dedicated Drum Channel
    
    // MIDI Map:
    // 35, 36 = Kick
    // 38, 40 = Snare
    // 42, 44, 46 = HiHat
    // 41, 43, 45, 47, 48, 50 = Toms
    // 49, 57 = Cymbals
    
    if (note == 35 || note == 36) {
        load_patch(8, &patch_bd);
    } 
    else if (note == 38 || note == 40) {
        load_patch(8, &patch_snare);
    }
    else if (note >= 41) {
        load_patch(8, &patch_hihat); // Lazy catch-all for cymbals/hats
    }
}

// Updated Sequencer
void play_song(const SongEvent* song) {
    int i = 0;
    while (true) {
        SongEvent event = song[i];
        
        if (event.delay_ms > 0) sleep_ms(event.delay_ms);

        if (event.type == 2) break; // End
        
        else if (event.type == 1) { // Note On
            // SPECIAL HANDLING FOR DRUMS (Ch 8)
            if (event.channel == 8) {
                handle_drum_note(event.note);
            }
            
            OPL_NoteOn(event.channel, event.note);
        }
        else if (event.type == 0) { // Note Off
            OPL_NoteOff(event.channel);
        }

        i++;
    }
}

int main() {
    stdio_init_all();
    setup_pins(); // Pins default to HIGH (Inactive)

    printf("Pico OPL2 Jukebox Starting...\n");

    // --- NEW: WAIT FOR FPGA TO WAKE UP ---
    // The TinyFPGA BX bootloader takes about 1-2 seconds.
    // We wait 3 seconds to be absolutely sure the FPGA is ready 
    // to listen to our commands.
    printf("Waiting for FPGA Bootloader...\n");
    sleep_ms(3000); 

    // 1. Hardware Reset (Pulse Low)
    // Now that the FPGA is definitely awake, we reset its core logic.
    printf("Resetting JTOPL Core...\n");
    gpio_put(PIN_RST, 0); 
    sleep_ms(10);
    gpio_put(PIN_RST, 1); 
    sleep_ms(10);

    // 2. Software Initialization
    printf("Initializing OPL2 Registers...\n");
    opl_clear();
    
    opl_write(false, 0x01); opl_write(true, 0x20); // Enable Waveforms

    // --- INSTRUMENT SETUP ---
    printf("Loading Instruments...\n");

    // 1. Clear all 9 channels to a default (Guitar)
    // This ensures unused channels don't make garbage sounds if accidentally triggered
    for(int i=0; i<9; i++) {
        load_patch(i, &patch_guitar);
    }

    // 2. Specific Assignments for Doom E1M1
    // The MIDI file uses Channel 1 for the Riff and Channel 2 for the Bass.
    // Our Python script maps these 1:1 to OPL channels.
    
    load_patch(1, &patch_guitar); // OPL Ch 1: Lead Guitar
    load_patch(2, &patch_bass);   // OPL Ch 2: Electric Bass (Quieter volume)

    // 3. Drum Channel (OPL Ch 8)
    // Even though 'handle_drum_note' swaps this dynamically during the song,
    // we load the Kick Drum initially so the channel isn't undefined.
    load_patch(8, &patch_bd);

    printf("Starting Jukebox...\n");

    while(true) {
        printf("Playing 'DOOM'...\n");
        play_song(doom_song);
        sleep_ms(2000); // Wait 2 seconds before repeating the song
    }
}