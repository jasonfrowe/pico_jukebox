import mido
import sys

# INSTRUMENT REMAP TABLE
INSTRUMENT_REMAP = {
    34: 34,  # Map Pick Bass (34) -> Finger Bass (33)
    30: 30,  # Map Distortion Gt (30) -> Overdriven Gt (29)
}

def midi_to_c(input_file, output_file, array_name="midi_song"):
    mid = mido.MidiFile(input_file)
    events = []
    
    print(f"Parsing {input_file}...")
    
    pending_time = 0.0

    for msg in mid:
        pending_time += msg.time
        
        # We look for note_on, note_off, program_change
        if msg.type not in ['note_on', 'note_off', 'program_change']:
            continue

        # Channel Mapping
        opl_ch = -1
        if msg.channel == 9: opl_ch = 8 
        # elif msg.channel == 2: continue # <--- MUTE 2nd Guitar (MIDI Ch 2 is index 1)
        elif msg.channel < 8: opl_ch = msg.channel
        else: continue

        # Command Parsing
        event_type = 0
        data_byte = 0
        velocity = 0 # Default

        if msg.type == 'program_change':
            if opl_ch == 8: continue # Ignore drum patch changes
            event_type = 3
            original = msg.program
            data_byte = INSTRUMENT_REMAP.get(original, original)
            
        elif msg.type == 'note_on' and msg.velocity > 0:
            event_type = 1
            data_byte = msg.note
            velocity = msg.velocity # Capture Velocity!
            
        else: # Note Off
            event_type = 0
            data_byte = msg.note
            velocity = 0

        # Generate C Struct
        delay_ms = int(pending_time * 1000)
        
        # { type, delay, ch, note, velocity }
        events.append(f"    {{ .type={event_type}, .delay_ms={delay_ms}, .channel={opl_ch}, .note={data_byte}, .velocity={velocity} }},")
        
        pending_time = 0.0

    # Write File
    with open(output_file, 'w') as f:
        f.write(f"#ifndef {array_name.upper()}_H\n#define {array_name.upper()}_H\n\n")
        f.write(f"const SongEvent {array_name}[] = {{\n")
        f.write("\n".join(events))
        f.write("\n    { .type=2, .delay_ms=0 } // End\n")
        f.write("};\n\n#endif\n")
    
    print(f"Done! Saved {len(events)} events.")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python midi2c.py <file.mid>")
    else:
        midi_to_c(sys.argv[1], "song_data.h", "midi_song")