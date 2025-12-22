#ifndef QUEUE_H
#define QUEUE_H

#include "pico/util/queue.h"

// The Data Packet the 6502 will send.
// 6 Bytes total per event.
typedef struct {
    uint8_t type;      // 1=NoteOn, 0=NoteOff, 3=PatchChange
    uint16_t delay_ms; // Wait time before executing this event
    uint8_t channel;   // 0-8
    uint8_t note;      // MIDI Note
    uint8_t velocity;  // 0-127
} SongEvent;

// Declare the queue globally so main.c can see it
extern queue_t event_queue;

#endif