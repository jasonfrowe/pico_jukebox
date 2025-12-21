import mido
import sys

def midi_to_c(input_file, output_file, array_name="midi_song"):
    mid = mido.MidiFile(input_file)
    events = []
    
    print(f"Parsing {input_file}...")
    
    pending_time = 0.0

    for msg in mid:
        pending_time += msg.time
        
        # Define Event Types
        # 0 = Note Off
        # 1 = Note On
        # 2 = End of Song
        # 3 = Program Change (New!)
        
        event_type = -1
        opl_ch = -1
        data_byte = 0 # Holds Note OR Program Number

        # --- 1. Filter & Map Channels ---
        # We process NoteOn, NoteOff, and ProgramChange
        if msg.type in ['note_on', 'note_off', 'program_change']:
            
            # Map Drums (MIDI Ch 9) -> OPL Ch 8
            if msg.channel == 9:
                opl_ch = 8
            # Map Melodic (MIDI Ch 0-8) -> OPL Ch 0-8
            elif msg.channel < 8:
                opl_ch = msg.channel
            else:
                # Skip channels > 8 (OPL2 limit)
                continue

            # --- 2. Determine Event Type ---
            if msg.type == 'program_change':
                event_type = 3
                data_byte = msg.program # The instrument number (0-127)
                
                # Ignore program changes on the Drum Channel (8)
                # (Standard MIDI drums don't usually change programs like melodic channels)
                if opl_ch == 8: 
                    continue

            elif msg.type == 'note_on' and msg.velocity > 0:
                event_type = 1
                data_byte = msg.note
                
            else: # Note Off (or Note On w/ vol=0)
                event_type = 0
                data_byte = msg.note

            # --- 3. Write Event ---
            delay_ms = int(pending_time * 1000)
            
            # Format: { delay, type, channel, note/program }
            events.append(f"    {{ .delay_ms={delay_ms}, .type={event_type}, .channel={opl_ch}, .note={data_byte} }},")
            
            pending_time = 0.0

    # Write C Header
    with open(output_file, 'w') as f:
        f.write(f"#ifndef {array_name.upper()}_H\n#define {array_name.upper()}_H\n\n")
        f.write("#include \"opl.h\"\n\n")
        f.write(f"const SongEvent {array_name}[] = {{\n")
        f.write("\n".join(events))
        f.write("\n    { .delay_ms=0, .type=2 } // End of Song\n")
        f.write("};\n\n#endif\n")
    
    print(f"Done! Saved {len(events)} events to {output_file}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python midi2c.py <file.mid>")
    else:
        midi_to_c(sys.argv[1], "song_data.h", "midi_song")