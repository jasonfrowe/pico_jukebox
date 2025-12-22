#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "opl.h"
#include "instruments.h"

// --- VELOCITY HELPER ---
// Scales volume based on your 0-255 input
void set_channel_volume(uint8_t channel, int user_volume) {
    if (channel > 8) return;
    
    // Clamp 0-255 to MIDI 0-127
    if (user_volume > 255) user_volume = 255;
    if (user_volume < 0) user_volume = 0;
    
    // Map 0-255 -> 0-127
    uint8_t velocity = user_volume >> 1; 
    
    // 1. Get Base Volume from Shadow Array (from instruments.c)
    uint8_t base_ksl = shadow_carrier_ksl[channel];
    uint8_t base_tl  = base_ksl & 0x3F; 
    uint8_t ksl_bits = base_ksl & 0xC0; 
    
    // 2. Calculate Attenuation (Invert Velocity)
    // 127 = 0 attenuation (Loud), 0 = 63 attenuation (Silent)
    uint8_t attenuation = (127 - velocity) >> 1; 
    
    uint8_t final_tl = base_tl + attenuation;
    if (final_tl > 63) final_tl = 63; 
    
    // 3. Write
    // We need to calculate the offset manually here since this logic isn't exposed in a helper
    uint8_t offsets[9] = {0, 1, 2, 8, 9, 10, 16, 17, 18};
    opl_write(false, 0x43 + offsets[channel]);
    opl_write(true,  ksl_bits | final_tl);
}

// --- INPUT HELPER ---
void read_line(char* buffer, int max_len) {
    int idx = 0;
    while (true) {
        int c = getchar(); // Blocks until character received
        if (c == '\r' || c == '\n') {
            buffer[idx] = 0; // Null terminate
            printf("\n");
            return;
        }
        if (c >= 32 && c <= 126 && idx < max_len - 1) {
            buffer[idx++] = (char)c;
            putchar(c);
        }
    }
}

// --- PATCH PARSER ---
// Updates a specific instrument in memory from a hex string
void parse_patch_string(uint8_t id, char* str) {
    OPL_Patch p;
    uint8_t bytes[11];
    int count = 0;
    
    char* token = strtok(str, ", ");
    while (token != NULL && count < 11) {
        bytes[count++] = (uint8_t)strtol(token, NULL, 0); 
        token = strtok(NULL, ", ");
    }
    
    if (count == 11) {
        p.m_ave = bytes[0]; p.m_ksl = bytes[1]; p.m_atdec = bytes[2]; p.m_susrel = bytes[3]; p.m_wave = bytes[4];
        p.c_ave = bytes[5]; p.c_ksl = bytes[6]; p.c_atdec = bytes[7]; p.c_susrel = bytes[8]; p.c_wave = bytes[9];
        p.feedback = bytes[10];
        
        update_gm_patch(id, &p);
        printf("Updated Inst %d in RAM.\n", id);
    } else {
        printf("Error: Expected 11 bytes, got %d.\n", count);
    }
}

// --- ATTENUATION HELPER ---
// Updates just the volume of an instrument
void update_attenuation(uint8_t id, int attn) {
    if (id > 127) return;
    if (attn < 0) attn = 0;
    if (attn > 63) attn = 63;

    // Read current from RAM
    OPL_Patch p = gm_bank[id];
    
    // Update TL
    uint8_t ksl_bits = p.c_ksl & 0xC0;
    p.c_ksl = ksl_bits | (uint8_t)attn;
    
    update_gm_patch(id, &p);
    
    printf("Updated Inst %d: Attenuation set to %d.\n", id, attn);
}

int main() {
    stdio_init_all();
    
    // Use the library function to setup pins!
    setup_pins();

    sleep_ms(2000);
    printf("\n=== OPL2 LIVE TUNER ===\n");
    printf("1. Play:   [INST] [NOTE] [VOL 0-255]  (Ex: 29 50 255)\n");
    printf("2. Patch:  P [INST] [BYTES]           (Ex: P 0 0x01,0x10...)\n");
    printf("3. Volume: A [INST] [LEVEL 0-63]      (Ex: A 29 12)\n");

    // FPGA Reset
    printf("Resetting FPGA...\n");
    gpio_put(PIN_RST, 0); sleep_ms(10); 
    gpio_put(PIN_RST, 1); sleep_ms(10);
    
    // OPL Init
    opl_clear();
    opl_write(false, 0x01); opl_write(true, 0x20); // Enable Waveforms

    char input_buf[128];

    while (true) {
        printf("> ");
        read_line(input_buf, 128);

        // --- 'P' PATCH COMMAND ---
        if (input_buf[0] == 'P' || input_buf[0] == 'p') {
            int inst_id;
            char* data_start = strchr(input_buf, ' ');
            if (data_start) {
                inst_id = strtol(data_start, &data_start, 0);
                parse_patch_string((uint8_t)inst_id, data_start);
            }
        } 
        // --- 'A' ATTENUATION COMMAND ---
        else if (input_buf[0] == 'A' || input_buf[0] == 'a') {
            int inst_id, attn_val;
            if (sscanf(input_buf + 1, "%d %d", &inst_id, &attn_val) == 2) {
                update_attenuation((uint8_t)inst_id, attn_val);
            } else {
                printf("Usage: A [INST_ID] [VALUE 0-63]\n");
            }
        }
        // --- PLAY COMMAND ---
        else {
            int inst, note, vol;
            if (sscanf(input_buf, "%d %d %d", &inst, &note, &vol) == 3) {
                printf("Playing I:%d N:%d V:%d\n", inst, note, vol);
                
                // 1. Reload patch to apply any updates
                // Uses Channel 0 for testing
                load_gm_instrument(0, (uint8_t)inst); 
                
                // 2. Apply Velocity scaling
                set_channel_volume(0, vol);
                
                // 3. Play
                OPL_NoteOn(0, (uint8_t)note);
                
                // 4. Hold
                sleep_ms(1000);
                
                // 5. Release
                OPL_NoteOff(0);
            }
        }
    }
}