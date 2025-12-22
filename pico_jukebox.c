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

// --- HELPER: Velocity Scaling ---
void apply_velocity(uint8_t channel, uint8_t velocity) {
    if (channel > 8) return;
    
    // 1. Get Base Volume from Shadow Array (from instruments.c)
    uint8_t base_ksl = shadow_carrier_ksl[channel];
    uint8_t base_tl  = base_ksl & 0x3F; 
    uint8_t ksl_bits = base_ksl & 0xC0; 
    
    // 2. Calculate Attenuation (Invert MIDI Velocity)
    // 127=Loud (0 atten), 1=Quiet (~63 atten)
    uint8_t attenuation = (127 - velocity) >> 1; 
    
    // 3. Add to Base
    uint8_t final_tl = base_tl + attenuation;
    if (final_tl > 63) final_tl = 63; 
    
    // 4. Write to Carrier KSL Register
    uint8_t offsets[9] = {0, 1, 2, 8, 9, 10, 16, 17, 18};
    opl_write(false, 0x43 + offsets[channel]);
    opl_write(true,  ksl_bits | final_tl);
}

// --- THE ENGINE ---
void play_song(const SongEvent* song) {
    int i = 0;
    while (true) {
        SongEvent event = song[i];
        
        // 1. Wait
        if (event.delay_ms > 0) sleep_ms(event.delay_ms);

        // 2. Process
        switch (event.type) {
            case 0: // Note Off
                OPL_NoteOff(event.channel);
                break;

            case 1: // Note On
                // A. Drum Logic
                if (event.channel == 8) {
                    load_drum_patch(8, event.note);
                    // Pitch Hack: Make snare/hats crisp
                    if(event.note > 36) event.note = 60; 
                }
                
                // B. Dynamics
                apply_velocity(event.channel, event.velocity);
                
                // C. Play
                OPL_NoteOn(event.channel, event.note);
                break;

            case 2: // End
                return; 

            case 3: // Program Change
                // Doom Fix: Force heavy overdrive (29) instead of weak distortion (30)
                if (event.note == 30) load_gm_instrument(event.channel, 29);
                // Doom Fix: Force finger bass (33) if file asks for pick bass (34)
                else if (event.note == 34) load_gm_instrument(event.channel, 33);
                // Normal Load
                else load_gm_instrument(event.channel, event.note);
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

        // Reset state between loops
        for(int c=0; c<9; c++) OPL_NoteOff(c);
        load_drum_patch(8, 36);

        sleep_ms(2000);
    }
}
