#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "opl.h"
#include "instruments.h"

// --- HARDWARE CONFIGURATION ---
#define PIN_A0  8   
#define PIN_WE  9   
#define PIN_CS  10  
#define PIN_RST 11  
#define DATA_MASK 0xFF

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

void setup_pins() {
    for(int i=0; i<8; i++) {
        gpio_init(i); gpio_set_dir(i, GPIO_OUT);
    }
    int controls[] = {PIN_A0, PIN_WE, PIN_CS, PIN_RST};
    for(int i=0; i<4; i++) {
        gpio_init(controls[i]); gpio_set_dir(controls[i], GPIO_OUT);
        gpio_put(controls[i], 1); 
    }
}

// --- VELOCITY HELPER ---
void set_channel_volume(uint8_t channel, int user_volume) {
    if (channel > 8) return;
    if (user_volume > 127) user_volume = 127;
    if (user_volume < 0) user_volume = 0;
    
    // Use the shadow register (which holds the Patch's "Master Volume")
    uint8_t base_ksl = shadow_carrier_ksl[channel];
    uint8_t base_tl  = base_ksl & 0x3F; 
    uint8_t ksl_bits = base_ksl & 0xC0; 
    
    // Scale velocity on TOP of the patch volume
    uint8_t attenuation = (127 - user_volume) >> 1; 
    uint8_t final_tl = base_tl + attenuation;
    if (final_tl > 63) final_tl = 63; 
    
    uint8_t offsets[9] = {0, 1, 2, 8, 9, 10, 16, 17, 18};
    opl_write(false, 0x43 + offsets[channel]);
    opl_write(true,  ksl_bits | final_tl);
}

void read_line(char* buffer, int max_len) {
    int idx = 0;
    while (true) {
        int c = getchar();
        if (c == '\r' || c == '\n') {
            buffer[idx] = 0;
            printf("\n");
            return;
        }
        if (c >= 32 && c <= 126 && idx < max_len - 1) {
            buffer[idx++] = (char)c;
            putchar(c);
        }
    }
}

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
        printf("Updated Inst %d (Patch).\n", id);
    } else {
        printf("Error: Expected 11 bytes.\n");
    }
}

// --- NEW ATTENUATION HELPER ---
void update_attenuation(uint8_t id, int attn) {
    if (id > 127) return;
    if (attn < 0) attn = 0;
    if (attn > 63) attn = 63;

    // Get current patch
    OPL_Patch p = gm_bank[id];
    
    // Preserve KSL bits (Top 2), Replace TL bits (Bottom 6) with new attenuation
    uint8_t ksl_bits = p.c_ksl & 0xC0;
    p.c_ksl = ksl_bits | (uint8_t)attn;
    
    // Write back to Bank
    update_gm_patch(id, &p);
    
    printf("Updated Inst %d: Attenuation set to %d (Reg: 0x%02X)\n", id, attn, p.c_ksl);
}

int main() {
    stdio_init_all();
    setup_pins();

    sleep_ms(2000);
    printf("\n=== OPL2 LIVE TUNER ===\n");
    printf("1. Play:   [INST] [NOTE] [VOL]  (Ex: 29 50 127)\n");
    printf("2. Patch:  P [INST] [BYTES]     (Ex: P 0 0x01...)\n");
    printf("3. Volume: A [INST] [LEVEL]     (Ex: A 29 12) -> Set Atten to 12\n");

    // Init
    gpio_put(PIN_RST, 0); sleep_ms(10); gpio_put(PIN_RST, 1); sleep_ms(10);
    opl_clear();
    opl_write(false, 0x01); opl_write(true, 0x20);

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
            // Skip the 'A' and parse numbers
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
                
                // Reload patch to apply any 'A' updates
                load_gm_instrument(0, (uint8_t)inst); 
                
                // Apply Velocity scaling
                set_channel_volume(0, vol);
                
                OPL_NoteOn(0, (uint8_t)note);
                sleep_ms(1000);
                OPL_NoteOff(0);
            }
        }
    }
}