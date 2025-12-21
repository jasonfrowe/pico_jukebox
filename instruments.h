#ifndef INSTRUMENTS_H
#define INSTRUMENTS_H

#include <stdint.h>
#include "pico/stdlib.h"
#include "opl.h"

typedef struct {
    // Modulator (Op 1)
    uint8_t m_ave;   // 20-35: Mult/Vibrato
    uint8_t m_ksl;   // 40-55: Level/KSL (00=Loudest, 3F=Quiestest)
    uint8_t m_atdec; // 60-75: Attack/Decay
    uint8_t m_susrel;// 80-95: Sustain/Release
    uint8_t m_wave;  // E0-F5: Waveform

    // Carrier (Op 2)
    uint8_t c_ave;   
    uint8_t c_ksl;   // TL (Total Level) is here!
    uint8_t c_atdec; 
    uint8_t c_susrel;
    uint8_t c_wave;  

    uint8_t feedback; // C0-C8
} OPL_Patch;

// --- MELODIC INSTRUMENTS ---

// 1. Electric Guitar (Clean-ish)
const OPL_Patch patch_guitar = {
    .m_ave=0x01, .m_ksl=0x15, .m_atdec=0xF4, .m_susrel=0xF4, .m_wave=0x01,
    .c_ave=0x01, .c_ksl=0x05, .c_atdec=0xF4, .c_susrel=0xF4, .c_wave=0x00, 
    .feedback=0x06 // Medium Feedback for "string" sound
};

// 2. Electric Bass (Punchy but Tamed)
const OPL_Patch patch_bass = {
    .m_ave=0x01, .m_ksl=0x1A, .m_atdec=0x66, .m_susrel=0xF6, .m_wave=0x01,
    .c_ave=0x00, .c_ksl=0x08, .c_atdec=0x94, .c_susrel=0xC4, .c_wave=0x00, 
    // ^ c_ksl=0x08 means "Turn down by 8 steps" (Quieter than guitar)
    .feedback=0x08
};

// --- DRUM INSTRUMENTS ---

// 3. Kick Drum (Deep Sine Wave, No Noise)
const OPL_Patch patch_bd = {
    .m_ave=0x00, .m_ksl=0x00, .m_atdec=0xF1, .m_susrel=0xCF, .m_wave=0x00,
    .c_ave=0x00, .c_ksl=0x00, .c_atdec=0xF1, .c_susrel=0xCF, .c_wave=0x00,
    .feedback=0x00
};

// 4. Snare Drum (White Noise)
const OPL_Patch patch_snare = {
    .m_ave=0x08, .m_ksl=0x00, .m_atdec=0xF9, .m_susrel=0xF9, .m_wave=0x00,
    .c_ave=0x01, .c_ksl=0x00, .c_atdec=0xF9, .c_susrel=0xF9, .c_wave=0x02, // 0x02 = White Noise Wave
    .feedback=0x00
};

// 5. Hi-Hat (High Pitch Noise)
const OPL_Patch patch_hihat = {
    .m_ave=0x01, .m_ksl=0x00, .m_atdec=0xF8, .m_susrel=0xF8, .m_wave=0x00,
    .c_ave=0x01, .c_ksl=0x00, .c_atdec=0xF8, .c_susrel=0xF8, .c_wave=0x02, // Noise
    .feedback=0x00
};

// Helper to load a patch
void load_patch(uint8_t ch, const OPL_Patch* p) {
    if (ch > 8) return;
    uint8_t offsets[9] = {0, 1, 2, 8, 9, 10, 16, 17, 18};
    uint8_t off = offsets[ch];

    opl_write(false, 0x20 + off); opl_write(true, p->m_ave);
    opl_write(false, 0x40 + off); opl_write(true, p->m_ksl);
    opl_write(false, 0x60 + off); opl_write(true, p->m_atdec);
    opl_write(false, 0x80 + off); opl_write(true, p->m_susrel);
    opl_write(false, 0xE0 + off); opl_write(true, p->m_wave);

    opl_write(false, 0x23 + off); opl_write(true, p->c_ave);
    opl_write(false, 0x43 + off); opl_write(true, p->c_ksl);
    opl_write(false, 0x63 + off); opl_write(true, p->c_atdec);
    opl_write(false, 0x83 + off); opl_write(true, p->c_susrel);
    opl_write(false, 0xE3 + off); opl_write(true, p->c_wave);

    opl_write(false, 0xC0 + ch);  opl_write(true, p->feedback);
}
#endif