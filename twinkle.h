#ifndef TWINKLE_H
#define TWINKLE_H

#include "opl.h"

// "Twinkle Twinkle Little Star" song data
const SongEvent twinkle_star[] = {
    // C C
    { .delay_ms=0,   .type=1, .channel=0, .note=72 }, 
    { .delay_ms=400, .type=0, .channel=0, .note=0 }, // NoteOff doesn't need a note number
    { .delay_ms=50,  .type=1, .channel=0, .note=72 },
    { .delay_ms=400, .type=0, .channel=0, .note=0 },
    
    // G G
    { .delay_ms=50,  .type=1, .channel=0, .note=79 },
    { .delay_ms=400, .type=0, .channel=0, .note=0 },
    { .delay_ms=50,  .type=1, .channel=0, .note=79 },
    { .delay_ms=400, .type=0, .channel=0, .note=0 },

    // A A
    { .delay_ms=50,  .type=1, .channel=0, .note=81 },
    { .delay_ms=400, .type=0, .channel=0, .note=0 },
    { .delay_ms=50,  .type=1, .channel=0, .note=81 },
    { .delay_ms=400, .type=0, .channel=0, .note=0 },

    // G (Long)
    { .delay_ms=50,  .type=1, .channel=0, .note=79 },
    { .delay_ms=800, .type=0, .channel=0, .note=0 },

    // End of song marker
    { .delay_ms=0,   .type=2 } 
};

#endif // TWINKLE_H