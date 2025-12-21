import mido
import sys

def midi_to_c(input_file, output_file, array_name="midi_song"):
    mid = mido.MidiFile(input_file)
    events = []
    
    # Merge all tracks into a single timeline
    # This automatically handles delta-time conversion
    print(f"Parsing {input_file}...")

    # We only care about Note On/Off for now
    # We will map MIDI Ch 10 (Drums) to OPL Ch 8 (Last channel)
    # We will map MIDI Ch 1-8 to OPL Ch 0-7
    
    for msg in mid:
        if msg.type == 'note_on' or msg.type == 'note_off':
            
            # 1. Delta Time Conversion (Seconds -> ms)
            delay_ms = int(msg.time * 1000)
            
            # 2. Channel Mapping
            # MIDI uses 0-15. OPL uses 0-8.
            # Map Drums (9 in 0-indexed MIDI) to 8
            if msg.channel == 9: 
                opl_ch = 8 
            elif msg.channel < 8:
                opl_ch = msg.channel
            else:
                continue # Skip channels > 8
            
            # 3. Command Type
            # NoteOn with Velocity 0 is actually NoteOff
            cmd = 1 # Note On
            if msg.type == 'note_off' or (msg.type == 'note_on' and msg.velocity == 0):
                cmd = 0 # Note Off

            events.append(f"    {{ .delay_ms={delay_ms}, .type={cmd}, .channel={opl_ch}, .note={msg.note} }},")

    # Write C Header
    with open(output_file, 'w') as f:
        f.write(f"#ifndef {array_name.upper()}_H\n#define {array_name.upper()}_H\n\n")
        f.write("#include \"opl.h\"\n\n")
        f.write(f"const SongEvent {array_name}[] = {{\n")
        f.write("\n".join(events))
        f.write("\n    { .delay_ms=0, .type=2 } // End of Song\n") # Terminator
        f.write("};\n\n#endif\n")
    
    print(f"Done! Saved {len(events)} events to {output_file}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python midi2c.py <file.mid>")
    else:
        midi_to_c(sys.argv[1], "song_data.h", "doom_song")