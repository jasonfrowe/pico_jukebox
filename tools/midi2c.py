import mido
import sys

def midi_to_c(input_file, output_file, array_name="midi_song"):
    mid = mido.MidiFile(input_file)
    events = []
    
    print(f"Parsing {input_file}...")
    
    # ACCUMULATOR FOR SKIPPED TIME
    pending_time = 0.0

    for msg in mid:
        # Always add the time of the current message to our accumulator
        pending_time += msg.time
        
        # Filter: We only want Note On/Off
        if msg.type == 'note_on' or msg.type == 'note_off':
            
            # --- CHANNEL MAPPING ---
            # Map MIDI Ch 10 (9) -> OPL Ch 8
            if msg.channel == 9: 
                opl_ch = 8 
            elif msg.channel < 8:
                opl_ch = msg.channel
            else:
                # If we skip this channel, we must NOT reset pending_time yet!
                # We just loop again, keeping the time accumulated.
                continue 
            
            # --- COMMAND PARSING ---
            cmd = 1 # Note On
            if msg.type == 'note_off' or (msg.type == 'note_on' and msg.velocity == 0):
                cmd = 0 # Note Off

            # --- GENERATE EVENT ---
            # Now we use the accumulated time
            delay_ms = int(pending_time * 1000)
            
            events.append(f"    {{ .delay_ms={delay_ms}, .type={cmd}, .channel={opl_ch}, .note={msg.note} }},")
            
            # Reset accumulator because we just "spent" the time
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
        midi_to_c(sys.argv[1], "song_data.h", "doom_song")