import sys
import struct

def parse_wopl(filename, output_c="instruments_wopl.c"):
    print(f"Reading {filename}...")
    
    with open(filename, "rb") as f:
        data = f.read()

    if data[0:11] != b'WOPL3-BANK\x00':
        print("Error: Not a valid WOPL file.")
        return

    # Use the Force-Scan logic to get the start
    header_size = 19
    # WOPL usually has 2 bank names (Melodic + Perc) even if Perc is empty
    bank_names_size = 64 
    start_offset = header_size + bank_names_size
    
    # Based on your scan, Stride is 66
    stride = 66
    
    c_output = []
    
    for i in range(128):
        base = start_offset + (i * stride)
        
        # --- NAME ---
        name_raw = data[base+4 : base+32] 
        name = name_raw.decode('ascii', 'ignore').strip().replace('\x00', '')
        if not name: name = f"Inst {i}"

        # --- DECODED OFFSETS (CORRECTED) ---
        # Feedback is at +44
        fb_conn = data[base + 44]
        
        # FIRST BLOCK (+46) IS CARRIER (Op 2)
        # Based on your Furnace inspection (TL=0, AD=D2)
        c_ave = data[base + 46]
        c_ksl = data[base + 47]
        c_atd = data[base + 48]
        c_sus = data[base + 49]
        c_wav = data[base + 50]
        
        # SECOND BLOCK (+51) IS MODULATOR (Op 1)
        # Based on your Furnace inspection (TL=4B, AD=F1)
        m_ave = data[base + 51]
        m_ksl = data[base + 52]
        m_atd = data[base + 53]
        m_sus = data[base + 54]
        m_wav = data[base + 55]

        # Format C Struct (Now mapped correctly)
        c_line = (f"    [{i}] = {{ .m_ave=0x{m_ave:02X}, .m_ksl=0x{m_ksl:02X}, .m_atdec=0x{m_atd:02X}, .m_susrel=0x{m_sus:02X}, .m_wave=0x{m_wav:02X}, "
                  f".c_ave=0x{c_ave:02X}, .c_ksl=0x{c_ksl:02X}, .c_atdec=0x{c_atd:02X}, .c_susrel=0x{c_sus:02X}, .c_wave=0x{c_wav:02X}, "
                  f".feedback=0x{fb_conn:02X} }}, // {name}")
        
        c_output.append(c_line)

    # WRITE FILE
    with open(output_c, "w") as f:
        f.write('#include "instruments.h"\n\n')
        f.write(f'// Extracted from {filename}\n')
        f.write('uint8_t shadow_carrier_ksl[9] = {0};\n\n')
        f.write('OPL_Patch gm_bank[128] = {\n')
        f.write("\n".join(c_output))
        f.write('\n};\n')
        
        # Append Good Drums
        f.write('\n// Manual Drums (Using your tuned set)\n')
        f.write('const OPL_Patch drum_bd    = { .m_ave=0x00, .m_ksl=0x0D, .m_atdec=0xE8, .m_susrel=0xEF, .m_wave=0x00, .c_ave=0x00, .c_ksl=0x00, .c_atdec=0xA5, .c_susrel=0xFF, .c_wave=0x00, .feedback=0x06 };\n')
        f.write('const OPL_Patch drum_snare = { .m_ave=0x06, .m_ksl=0x00, .m_atdec=0xF0, .m_susrel=0xF0, .m_wave=0x00, .c_ave=0x00, .c_ksl=0x00, .c_atdec=0xF7, .c_susrel=0xF7, .c_wave=0x00, .feedback=0x0E };\n')
        f.write('const OPL_Patch drum_hihat = { .m_ave=0x05, .m_ksl=0x00, .m_atdec=0xF0, .m_susrel=0x77, .m_wave=0x00, .c_ave=0x00, .c_ksl=0x00, .c_atdec=0xFA, .c_susrel=0xEA, .c_wave=0x00, .feedback=0x0E };\n')
        
        # Helpers
        f.write('\n// --- HELPERS ---\n')
        f.write('void write_patch_to_channel(uint8_t ch, const OPL_Patch* p) {\n')
        f.write('    if (ch > 8) return;\n')
        f.write('    uint8_t offsets[9] = {0, 1, 2, 8, 9, 10, 16, 17, 18};\n')
        f.write('    uint8_t off = offsets[ch];\n')
        f.write('    opl_write(false, 0x20 + off); opl_write(true, p->m_ave);\n')
        f.write('    opl_write(false, 0x40 + off); opl_write(true, p->m_ksl);\n')
        f.write('    opl_write(false, 0x60 + off); opl_write(true, p->m_atdec);\n')
        f.write('    opl_write(false, 0x80 + off); opl_write(true, p->m_susrel);\n')
        f.write('    opl_write(false, 0xE0 + off); opl_write(true, p->m_wave);\n')
        f.write('    shadow_carrier_ksl[ch] = p->c_ksl;\n')
        f.write('    opl_write(false, 0x23 + off); opl_write(true, p->c_ave);\n')
        f.write('    opl_write(false, 0x43 + off); opl_write(true, p->c_ksl);\n')
        f.write('    opl_write(false, 0x63 + off); opl_write(true, p->c_atdec);\n')
        f.write('    opl_write(false, 0x83 + off); opl_write(true, p->c_susrel);\n')
        f.write('    opl_write(false, 0xE3 + off); opl_write(true, p->c_wave);\n')
        f.write('    opl_write(false, 0xC0 + ch);  opl_write(true, p->feedback);\n')
        f.write('}\n')
        
        f.write('void load_gm_instrument(uint8_t channel, uint8_t program_number) {\n')
        f.write('    if (program_number > 127) program_number = 0;\n')
        f.write('    write_patch_to_channel(channel, &gm_bank[program_number]);\n')
        f.write('}\n')
        
        f.write('void load_drum_patch(uint8_t channel, uint8_t drum_note) {\n')
        f.write('    if (drum_note == 35 || drum_note == 36) write_patch_to_channel(channel, &drum_bd);\n')
        f.write('    else if (drum_note == 38 || drum_note == 40) write_patch_to_channel(channel, &drum_snare);\n')
        f.write('    else write_patch_to_channel(channel, &drum_hihat);\n')
        f.write('}\n')
        
        f.write('void update_gm_patch(uint8_t program_number, const OPL_Patch* new_patch) {\n')
        f.write('    if (program_number > 127) return;\n')
        f.write('    gm_bank[program_number] = *new_patch;\n')
        f.write('    shadow_carrier_ksl[program_number] = new_patch->c_ksl;\n')
        f.write('}\n')

    print(f"Success! Generated {output_c}")

if __name__ == "__main__":
    parse_wopl("Apogee-IMF-90.wopl")