#ifndef INSTRUMENTS_H
#define INSTRUMENTS_H

#include <stdint.h>
#include <stdbool.h>
#include "opl.h"

// The Standard OPL2 Patch Structure
typedef struct {
    uint8_t m_ave, m_ksl, m_atdec, m_susrel, m_wave;
    uint8_t c_ave, c_ksl, c_atdec, c_susrel, c_wave;
    uint8_t feedback;
} OPL_Patch;

// --- Public Functions ---

// Load a specific General MIDI instrument (0-127) into a channel
extern void load_gm_instrument(uint8_t channel, uint8_t program_number);

// Load a specific Drum sound (Bass, Snare, HiHat, etc.)
extern void load_drum_patch(uint8_t channel, uint8_t drum_note);

#endif