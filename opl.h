#ifndef OPL_H
#define OPL_H


extern void opl_write(bool is_data, uint8_t data);
extern void setup_pins();
extern void OPL_NoteOn(uint8_t channel, uint8_t midi_note);
extern void OPL_NoteOff(uint8_t channel);
extern uint16_t midi_to_opl_freq(uint8_t midi_note);
extern void opl_clear();

extern uint8_t shadow_b0[9];

// --- Data Structures ---
typedef struct {
    uint8_t type;      // 1=NoteOn, 0=NoteOff, 3=PatchChange
    uint16_t delay_ms; 
    uint8_t channel;   
    uint8_t note;      
    uint8_t velocity;  // Added for dynamics
} SongEvent;


#endif // OPL_H