#ifndef OPL_H
#define OPL_H

extern void opl_write(bool is_data, uint8_t data);
extern void setup_pins();
extern void OPL_NoteOn(uint8_t channel, uint8_t midi_note);
extern void OPL_NoteOff(uint8_t channel);
extern uint16_t midi_to_opl_freq(uint8_t midi_note);
extern void opl_clear();

extern uint8_t shadow_b0[9];

// A simple music event structure
typedef struct {
    uint16_t delay_ms; // Milliseconds to wait before this event
    uint8_t type;      // 1 = NoteOn, 0 = NoteOff, 2 = End of Song
    uint8_t channel;   // 0-8
    uint8_t note;      // MIDI Note (0-127)
} SongEvent;

#endif // OPL_H