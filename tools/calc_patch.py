def get_byte(prompt, min_val=0, max_val=255):
    while True:
        try:
            val = int(input(f"{prompt}: "))
            if min_val <= val <= max_val: return val
            print(f"  Error: Must be between {min_val} and {max_val}")
        except ValueError:
            print("  Error: Enter a number")

def pack_op(name):
    print(f"\n--- {name} SETTINGS ---")
    
    # Register 0x20: Flags + Multiplier
    print(f"Flags (1=On, 0=Off):")
    am   = get_byte("  AM  (Tremolo) ", 0, 1)
    vib  = get_byte("  VIB (Vibrato) ", 0, 1)
    egt  = get_byte("  EGT (Sustain) ", 0, 1)
    ksr  = get_byte("  KSR (KeyScal) ", 0, 1)
    mult = get_byte("  MULT (0-15)   ", 0, 15)
    ave = (am<<7) | (vib<<6) | (egt<<5) | (ksr<<4) | mult

    # Register 0x40: KSL + TL
    ksl = get_byte("  KSL (Level Scale 0-3)", 0, 3)
    tl  = get_byte("  TL  (Volume 0-63)    ", 0, 63)
    ksl_val = (ksl << 6) | tl

    # Register 0x60: Attack + Decay
    ar = get_byte("  AR  (Attack 0-15) ", 0, 15)
    dr = get_byte("  DR  (Decay 0-15)  ", 0, 15)
    atdec = (ar << 4) | dr

    # Register 0x80: Sustain + Release
    sl = get_byte("  SL  (Sustain 0-15)", 0, 15)
    rr = get_byte("  RR  (Release 0-15)", 0, 15)
    susrel = (sl << 4) | rr

    # Register 0xE0: Waveform
    ws = get_byte("  WS  (Wave 0-3 or 7)", 0, 7)
    
    return ave, ksl_val, atdec, susrel, ws

print("=== Furnace to OPL C-Struct Converter ===")

# 1. Modulator
m_ave, m_ksl, m_atdec, m_susrel, m_wave = pack_op("MODULATOR (Op 1)")

# 2. Carrier
c_ave, c_ksl, c_atdec, c_susrel, c_wave = pack_op("CARRIER (Op 2)")

# 3. Global
print("\n--- GLOBAL SETTINGS ---")
fb  = get_byte("  FB  (Feedback 0-7)  ", 0, 7)
con = get_byte("  CON (Connection 0-1)", 0, 1)
feedback = (fb << 1) | con

# 4. Output
print("\n=== COPY THIS INTO YOUR C/PYTHON CODE ===")
print(f"// Mod: M={m_ave&0xF} K={m_ksl>>6}/{m_ksl&0x3F} AD={m_atdec>>4}/{m_atdec&0xF} SR={m_susrel>>4}/{m_susrel&0xF} W={m_wave}")
print(f"// Car: M={c_ave&0xF} K={c_ksl>>6}/{c_ksl&0x3F} AD={c_atdec>>4}/{c_atdec&0xF} SR={c_susrel>>4}/{c_susrel&0xF} W={c_wave}")
print(f"// FB={fb} CON={con}")
print(f"{{ .m_ave=0x{m_ave:02X}, .m_ksl=0x{m_ksl:02X}, .m_atdec=0x{m_atdec:02X}, .m_susrel=0x{m_susrel:02X}, .m_wave=0x{m_wave:02X}, " 
      f".c_ave=0x{c_ave:02X}, .c_ksl=0x{c_ksl:02X}, .c_atdec=0x{c_atdec:02X}, .c_susrel=0x{c_susrel:02X}, .c_wave=0x{c_wave:02X}, "
      f".feedback=0x{feedback:02X} }};")
print("\nOr for gen_bank.py list:")
print(f"0x{m_ave:02X},0x{m_ksl:02X},0x{m_atdec:02X},0x{m_susrel:02X},0x{m_wave:02X}, 0x{c_ave:02X},0x{c_ksl:02X},0x{c_atdec:02X},0x{c_susrel:02X},0x{c_wave:02X}, 0x{feedback:02X},")