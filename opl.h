#ifndef OPL_H
#define OPL_H

extern void opl_write(bool is_data, uint8_t data);
extern void setup_pins();
extern void OPL_NoteOn(uint8_t channel, uint8_t midi_note);
extern void OPL_NoteOff(uint8_t channel);
extern uint16_t midi_to_opl_freq(uint8_t midi_note);
extern void opl_clear();

extern uint8_t shadow_b0;  // Global shadow for channel 0 B0 (frequency + block)

#endif // OPL_H