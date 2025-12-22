import mido
import sys
from mido import MidiFile, tempo2bpm

# =============================================================================
# MT-32 → OPL2 Instrument Remapping
# MT-32 patches are very different from GM. This table maps common MT-32
# programs to good-sounding OPL2 patches (from popular .WOPL banks like Timbres of Heaven).
# Adjust as needed based on your gm_bank[] array index.
# =============================================================================
MT32_TO_OPL2_REMAP = {
    # Pianos
    0: 0,    # Acoustic Piano → Acoustic Grand
    1: 1,    # Bright Piano
    2: 4,    # Electric Piano → E.Piano 1
    4: 4,    # Honky-tonk → E.Piano 1 (or keep 3 if you have it)
    5: 5,    # E.Piano 2
    6: 6,    # Harpsichord
    7: 7,    # Clavi

    # Chromatic Percussion
    8: 8,    # Celesta
    9: 9,    # Glockenspiel
    10: 10,  # Music Box
    11: 11,  # Vibraphone
    12: 12,  # Marimba
    13: 13,  # Xylophone
    14: 14,  # Tubular Bells

    # Organs
    16: 16,  # Drawbar Organ
    17: 17,  # Percussive Organ
    18: 18,  # Rock Organ
    19: 19,  # Church Organ

    # Guitars
    24: 24,  # Acoustic Guitar (nylon)
    25: 25,  # Acoustic Guitar (steel)
    26: 26,  # Electric Guitar (jazz)
    27: 27,  # Electric Guitar (clean)
    29: 29,  # Overdriven Guitar
    30: 30,  # Distortion Guitar

    # Basses
    32: 32,  # Acoustic Bass
    33: 33,  # Electric Bass (finger)
    34: 34,  # Electric Bass (pick)
    38: 38,  # Synth Bass 1
    39: 39,  # Synth Bass 2

    # Strings / Ensemble
    40: 40,  # Violin
    42: 42,  # Cello
    48: 48,  # String Ensemble 1
    49: 49,  # String Ensemble 2
    50: 50,  # Synth Strings 1
    51: 51,  # Synth Strings 2

    # Brass
    56: 56,  # Trumpet
    60: 60,  # French Horn
    61: 61,  # Brass Section

    # Reed
    64: 64,  # Soprano Sax
    65: 65,  # Alto Sax
    66: 66,  # Tenor Sax
    68: 68,  # Oboe
    71: 71,  # Clarinet

    # Synth Lead / Pad
    80: 80,  # Lead 1 (square)
    81: 81,  # Lead 2 (sawtooth)
    85: 85,  # Lead 6 (voice)
    88: 88,  # Pad 1 (new age)
    90: 90,  # Pad 3 (polysynth)
    95: 95,  # Pad 8 (sweep)

    # Sound Effects
    120: 120, # Guitar Fret Noise → keep or ignore
    123: 123, # Bird Tweet
    126: 126, # Helicopter
}

def midi_to_c(input_file, output_file, array_name="midi_song"):
    mid = mido.MidiFile(input_file)

    # Tempo handling
    tempo = mido.bpm2tempo(120)  # Default tempo
    tick_rate = mid.ticks_per_beat

    events = []
    pending_ticks = 0

    print(f"Parsing {input_file}... ({mid.length:.1f}s, {len(mid.tracks)} tracks)")

    for msg in mid:
        pending_ticks += msg.time

        if msg.type == 'set_tempo':
            tempo = msg.tempo

        if msg.type not in ['note_on', 'note_off', 'program_change']:
            continue

        # Channel mapping: MT-32 uses ch 2-10 (0-based: 1-9), ch 10 = drums
        if msg.channel == 9:  # MIDI channel 10 (1-based)
            opl_ch = 9  # Use rhythm mode or melodic fallback
        elif 1 <= msg.channel <= 8:  # Channels 2-9 → OPL channels 0-7
            opl_ch = msg.channel - 1
        else:
            continue  # Ignore channel 0 (often control) and >9

        event_type = 0
        data_byte = 0
        velocity = 0
        patch = 0

        if msg.type == 'program_change':
            if opl_ch == 9:  # Ignore drum program changes
                continue
            event_type = 3
            original = msg.program
            patch = MT32_TO_OPL2_REMAP.get(original, original)  # Remap MT-32 → OPL2
            data_byte = patch

        elif msg.type == 'note_on' and msg.velocity > 0:
            event_type = 1
            data_byte = msg.note
            velocity = msg.velocity

        elif msg.type == 'note_off' or (msg.type == 'note_on' and msg.velocity == 0):
            event_type = 0
            data_byte = msg.note
            velocity = 0

        # Convert ticks to ms
        ms_per_tick = tempo / 1_000_000 / tick_rate
        delay_ms = int(pending_ticks * ms_per_tick + 0.5)  # Round nearest
        pending_ticks = 0

        events.append(
            f"  {{ .type = {event_type}, .delay_ms = {delay_ms}, .channel = {opl_ch}, .note = {data_byte}, .velocity = {velocity} }},"
        )

    # Write C header
    with open(output_file, 'w') as f:
        f.write(f"#ifndef {array_name.upper()}_H\n")
        f.write(f"#define {array_name.upper()}_H\n\n")
        f.write(f"// Generated from {input_file}\n")
        f.write(f"// Duration: ~{mid.length:.1f}s\n\n")
        f.write(f"const SongEvent {array_name}[] = {{\n")
        f.write("\n".join(events))
        f.write("\n  { .type = 2, .delay_ms = 0 }  // End of song\n")
        f.write("};\n\n")
        f.write(f"#endif // {array_name.upper()}_H\n")

    print(f"Done! {len(events)} events written to {output_file}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python midi_to_c.py <input.mid>")
        sys.exit(1)
    midi_to_c(sys.argv[1], "song_data.h", "midi_song")