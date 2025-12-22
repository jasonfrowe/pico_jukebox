#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include "opl.h"
#include "instruments.h"
#include "queue.h"

// --- CONFIGURATION ---
// Set to 1 to play internal Doom song. 
// Set to 0 to listen to RP6502 via GPIO.
#define TEST_MODE 1 

#if TEST_MODE
#include "song_data.h" // Only needed for testing
#endif

// 2. 6502 Interface (External Bus)
#define PIN_REQ 27 
#define PIN_ACK 28

// --- VOICE ALLOCATOR ---
typedef struct {
    bool active;
    uint8_t midi_channel; // Which MIDI channel owns this voice?
    uint8_t midi_note;    // Which note is playing?
    uint32_t age;         // For "Note Stealing" (Simple LRU)
} OPLVoice;

OPLVoice voices[9];       // The 9 Physical OPL Channels
uint32_t note_counter = 0; // Global clock for age tracking

// Track the current Instrument assigned to each MIDI Channel
// so we can load it into the physical OPL voice on demand.
uint8_t midi_ch_program[16] = {0}; 

queue_t event_queue;

void init_voices() {
    for(int i=0; i<9; i++) {
        voices[i].active = false;
        voices[i].midi_channel = 255;
        voices[i].midi_note = 0;
        voices[i].age = 0;
    }
}

// Find a physical voice to play this MIDI note
int allocate_voice(uint8_t m_ch, uint8_t m_note) {
    // 1. Check for Retrigger (Same note, same channel)
    for(int i=0; i<9; i++) {
        if (voices[i].active && voices[i].midi_channel == m_ch && voices[i].midi_note == m_note) {
            voices[i].age = ++note_counter;
            return i;
        }
    }

    // 2. DRUM HANDLING (MIDI Ch 9 -> Physical Voice 8)
    if (m_ch == 9) {
        // Drums share Voice 8 monophonically.
        // We always steal it.
        voices[8].active = true;
        voices[8].midi_channel = 9;
        voices[8].midi_note = m_note;
        return 8; 
    }

    // 3. MELODIC HANDLING (Find free voice 0-7)
    // We do NOT check Voice 8 here, preserving it for drums.
    for(int i=0; i<8; i++) {
        if (!voices[i].active) {
            voices[i].active = true;
            voices[i].midi_channel = m_ch;
            voices[i].midi_note = m_note;
            voices[i].age = ++note_counter;
            return i;
        }
    }

    // 4. STEAL OLDEST (Voices 0-7 only)
    int oldest_idx = 0;
    uint32_t min_age = 0xFFFFFFFF;
    for(int i=0; i<8; i++) {
        if (voices[i].age < min_age) {
            min_age = voices[i].age;
            oldest_idx = i;
        }
    }
    
    voices[oldest_idx].midi_channel = m_ch;
    voices[oldest_idx].midi_note = m_note;
    voices[oldest_idx].age = ++note_counter;
    return oldest_idx;
}

// Find which physical voice is playing this note to stop it
int find_active_voice(uint8_t m_ch, uint8_t m_note) {
    if (m_ch == 9) return 8; // Drums always Ch 8

    for(int i=0; i<8; i++) {
        if (voices[i].active && voices[i].midi_channel == m_ch && voices[i].midi_note == m_note) {
            return i;
        }
    }
    return -1; // Not found (maybe already stolen/stopped)
}

// --- AUDIO HELPERS ---
void apply_velocity(uint8_t channel, uint8_t velocity) {
    if (channel > 8) return;
    uint8_t base_ksl = shadow_carrier_ksl[channel];
    uint8_t base_tl  = base_ksl & 0x3F; 
    uint8_t ksl_bits = base_ksl & 0xC0; 
    uint8_t attenuation = (127 - velocity) >> 1; 
    uint8_t final_tl = base_tl + attenuation;
    if (final_tl > 63) final_tl = 63; 
    
    uint8_t offsets[9] = {0, 1, 2, 8, 9, 10, 16, 17, 18};
    opl_write(false, 0x43 + offsets[channel]);
    opl_write(true,  ksl_bits | final_tl);
}

// --- CORE 1: THE AUDIO ENGINE ---
void core1_entry() {
    SongEvent event;
    init_voices();
    
    while (true) {
        queue_remove_blocking(&event_queue, &event);
            
        if (event.delay_ms > 0) sleep_ms(event.delay_ms);

        switch (event.type) {
            case 0: // Note Off
            {
                // Find which OPL voice is playing this note
                int voice = find_active_voice(event.channel, event.note);
                if (voice != -1) {
                    OPL_NoteOff(voice);
                    voices[voice].active = false;
                }
                break;
            }

            case 1: // Note On
            {
                // 1. Allocate Voice
                int voice = allocate_voice(event.channel, event.note);
                
                // 2. Load Instrument
                if (event.channel == 9) { // MIDI DRUMS (Channel 9)
                    load_drum_patch(voice, event.note);
                    
                    // KICK FIX: Down 1 Octave
                    // if (event.note == 35 || event.note == 36) {
                    //     if(event.note >= 12) event.note -= 12; 
                    // }
                } 
                else {
                    // MELODIC
                    uint8_t prog = midi_ch_program[event.channel];
                    // ... (Your Doom Remap Logic here) ...
                    load_gm_instrument(voice, prog);
                }

                // 3. Play
                apply_velocity(voice, event.velocity);
                OPL_NoteOn(voice, event.note);
                break;
            }

            case 3: // Program Change
                // Just store the assignment. Don't load to OPL yet.
                // We load it when a NoteOn allocates a specific voice.
                if (event.channel < 16) {
                    midi_ch_program[event.channel] = event.note;
                }
                break;
                
            case 2: // Reset
                for(int i=0; i<9; i++) OPL_NoteOff(i);
                init_voices();
                break;
        }
    }
}

// --- CORE 0 HELPER: READ BUS ---
uint8_t read_data_bus() {
    uint32_t all_pins = gpio_get_all();
    uint8_t data = 0;
    
    if (all_pins & (1<<16)) data |= 0x01;
    if (all_pins & (1<<17)) data |= 0x02;
    if (all_pins & (1<<18)) data |= 0x04;
    if (all_pins & (1<<19)) data |= 0x08;
    if (all_pins & (1<<20)) data |= 0x10;
    if (all_pins & (1<<21)) data |= 0x20;
    if (all_pins & (1<<22)) data |= 0x40;
    if (all_pins & (1<<26)) data |= 0x80; 
    
    return data;
}

// --- CORE 0: MAIN ---
int main() {
    stdio_init_all();
    setup_pins(); // FPGA Pins
    
    // Interface Pins
    uint data_pins[] = {16,17,18,19,20,21,22,26};
    for(int i=0; i<8; i++) {
        gpio_init(data_pins[i]);
        gpio_set_dir(data_pins[i], GPIO_IN);
    }
    gpio_init(PIN_REQ); gpio_set_dir(PIN_REQ, GPIO_IN);
    gpio_init(PIN_ACK); gpio_set_dir(PIN_ACK, GPIO_OUT);
    gpio_put(PIN_ACK, 1); 

    // Init OPL
    printf("Booting Sound Card (Mode: %s)...\n", TEST_MODE ? "INTERNAL TEST" : "EXTERNAL VIA");
    sleep_ms(3000); 
    gpio_put(PIN_RST, 0); sleep_ms(10); gpio_put(PIN_RST, 1);
    opl_clear();
    opl_write(false, 0x01); opl_write(true, 0x20);

    // Defaults
    for(int i=0; i<9; i++) load_gm_instrument(i, 0);
    load_drum_patch(8, 36);

    // Start Engine
    queue_init(&event_queue, sizeof(SongEvent), 512); 
    multicore_launch_core1(core1_entry);

#if TEST_MODE
    // --- INTERNAL JUKEBOX MODE ---
    // Simulates the RP6502 by stuffing the queue from ROM
    while(true) {
        printf("Core 0: Feeding Queue...\n");
        int i = 0;
        while(true) {
            SongEvent e = midi_song[i++];
            
            if (e.type == 2) {
                // End of song marker
                // Send a Reset command to Core 1
                SongEvent reset = { .type=2, .delay_ms=0 };
                queue_add_blocking(&event_queue, &reset);
                break;
            }
            
            // Push to queue. 
            // If queue is full (because Core 1 is sleeping on a delay), 
            // this function BLOCKS. This perfectly tests flow control.
            queue_add_blocking(&event_queue, &e);
        }
        
        printf("Song Done. Restarting in 2s...\n");
        sleep_ms(2000);
        // Reset defaults for next loop
        load_drum_patch(8, 36);
    }

#else
    // --- EXTERNAL SOUND CARD MODE ---
    // Listens to GPIO for data from RP6502
    printf("Ready for Data via GPIO.\n");

    uint8_t buffer[6];
    int byte_idx = 0;

    while(true) {
        // 1. Wait for REQ LOW
        while(gpio_get(PIN_REQ) == 1) tight_loop_contents();

        // 2. Read
        uint8_t byte = read_data_bus();
        buffer[byte_idx++] = byte;

        // 3. ACK LOW
        gpio_put(PIN_ACK, 0);

        // 4. Wait for REQ HIGH
        while(gpio_get(PIN_REQ) == 0) tight_loop_contents();

        // 5. ACK HIGH
        gpio_put(PIN_ACK, 1);

        // 6. Assemble Packet
        if (byte_idx >= 6) {
            SongEvent e;
            e.type     = buffer[0];
            e.delay_ms = (buffer[1] << 8) | buffer[2];
            e.channel  = buffer[3];
            e.note     = buffer[4];
            e.velocity = buffer[5];
            
            queue_add_blocking(&event_queue, &e);
            byte_idx = 0;
        }
    }
#endif
}