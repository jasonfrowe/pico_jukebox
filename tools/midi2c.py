import mido
import sys

# --- CONFIGURATION ---
# Default to GM (No translation needed usually)
USE_MT32_MAP = False

# --- MAPS ---

# Standard Remaps for Doom/GM (Fixes weak patches)
GM_FIX_MAP = {
    30: 30, # Distortion Gt -> Overdriven Gt
    34: 34, # Pick Bass -> Finger Bass
}

# The "Rosetta Stone": MT-32 Preset -> General MIDI Program
# Based on the standard Roland MT-32 Preset list.
MT32_TO_GM = {
    # Pianos
    0: 0,   1: 1,   2: 3,   3: 4,   4: 5,   5: 6,   6: 7,   7: 2,
    # Organs
    8: 16,  9: 17,  10: 18, 11: 19, 12: 20, 13: 21, 14: 22, 15: 23,
    # Keyboards / Synths
    16: 62, 17: 63, 18: 38, 19: 39, 20: 80, 21: 81, 22: 54, 23: 55,
    # Bass
    24: 32, 25: 33, 26: 34, 27: 35, 28: 36, 29: 37, 30: 38, 31: 39,
    # Winds / Brass
    32: 56, 33: 57, 34: 58, 35: 59, 36: 60, 37: 61, 38: 62, 39: 63,
    40: 64, 41: 65, 42: 66, 43: 67, 44: 68, 45: 69, 46: 70, 47: 71,
    48: 72, 49: 73, 50: 74, 51: 75, 52: 76, 53: 77, 54: 78, 55: 79,
    # Strings
    56: 40, 57: 41, 58: 42, 59: 43, 60: 44, 61: 45, 62: 46, 63: 47,
    # Guitars
    64: 24, 65: 25, 66: 26, 67: 27, 68: 28, 69: 29, 70: 30, 71: 31,
    # Effects / Ethnic (Best Guesses)
    72: 104, 73: 105, 74: 106, 75: 107, 76: 108, 77: 109, 78: 110, 79: 111,
    # ... Many MT-32 patches don't map cleanly, but this covers the basics.
}

def midi_to_c(input_file, output_file, array_name="midi_song"):
    mid = mido.MidiFile(input_file)
    events = []
    
    print(f"Parsing {input_file} (MT-32 Mode: {USE_MT32_MAP})...")
    
    pending_time = 0.0

    for msg in mid:
        pending_time += msg.time
        
        if msg.type not in ['note_on', 'note_off', 'program_change']:
            continue

        # --- NEW CHANNEL MAPPING FOR POLYPHONY ---
        # We pass the MIDI channel "Raw" so the C code knows what it is.
        # MIDI Ch 10 = Index 9.
        
        opl_ch = msg.channel 
        
        # FILTER:
        # We only want Channels 0-8 (Melodic) and 9 (Drums).
        # We skip 10-15 because we don't have enough hardware voices to care.
        if opl_ch > 9: 
            continue 

        # ... Command Logic ...
        event_type = 0
        data_byte = 0
        velocity = 0

        if msg.type == 'program_change':
            if opl_ch == 9: continue # Ignore program changes for Drums
            
            event_type = 3
            original = msg.program
            
            # 1. MT-32 Translation
            if USE_MT32_MAP:
                # Look up the GM equivalent. Default to original if not found.
                translated = MT32_TO_GM.get(original, original)
                # Then apply the GM Fixes (e.g. Bass volume)
                data_byte = GM_FIX_MAP.get(translated, translated)
            else:
                # Direct GM mapping
                data_byte = GM_FIX_MAP.get(original, original)
            
        elif msg.type == 'note_on' and msg.velocity > 0:
            event_type = 1
            data_byte = msg.note
            velocity = msg.velocity
            
        else: # Note Off
            event_type = 0
            data_byte = msg.note
            velocity = 0

        # --- Output ---
        delay_ms = int(pending_time * 1000)
        events.append(f"    {{ .type={event_type}, .delay_ms={delay_ms}, .channel={opl_ch}, .note={data_byte}, .velocity={velocity} }},")
        pending_time = 0.0

    with open(output_file, 'w') as f:
        f.write(f"#ifndef {array_name.upper()}_H\n#define {array_name.upper()}_H\n\n")
        f.write('#include "opl.h"\n\n') # Use opl.h for the struct
        f.write(f"const SongEvent {array_name}[] = {{\n")
        f.write("\n".join(events))
        f.write("\n    { .type=2, .delay_ms=0 } // End\n")
        f.write("};\n\n#endif\n")
    
    print(f"Done! Saved {len(events)} events.")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python midi2c.py <file.mid> [--mt32]")
    else:
        # Simple flag check
        if "--mt32" in sys.argv:
            USE_MT32_MAP = True
            # Remove flag from args so filenames align
            sys.argv.remove("--mt32")
            
        midi_to_c(sys.argv[1], "song_data.h", "midi_song")