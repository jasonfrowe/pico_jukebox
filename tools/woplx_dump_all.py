import re

def parse_kv_list(s):
    items = {}
    for part in s.split(";"):
        part = part.strip()
        if not part:
            continue
        k, v = part.split("=")
        items[k.strip()] = int(v.strip())
    return items

def op_bytes(op):
    ave = (op["AM"] << 7) | (op["VB"] << 6) | (op["EG"] << 5) | (op["KR"] << 4) | op["ML"]
    ksl = (op["KL"] << 6) | op["TL"]
    atd = (op["AT"] << 4) | op["DC"]
    sr  = (op["ST"] << 4) | op["RL"]
    wf  = op["WF"]
    return [ave, ksl, atd, sr, wf]

def extract_section(text, start, end):
    lines = text.splitlines()
    in_section = False
    out = []

    for line in lines:
        line = line.rstrip()

        if line.startswith(start):
            in_section = True
            continue

        if in_section and line.startswith(end):
            break

        if in_section:
            out.append(line)

    return "\n".join(out)

def parse_instruments(text):
    instruments = []
    blocks = re.split(r"\n\s*\n", text.strip())

    for block in blocks:
        inst = {}
        for line in block.splitlines():
            line = line.strip()

            if line.startswith("INSTRUMENT="):
                inst["index"] = int(line.split("=")[1].rstrip(":"))

            elif line.startswith("NAME="):
                inst["name"] = line.split("=", 1)[1]

            elif line.startswith("FBCONN:"):
                inst["fbconn"] = parse_kv_list(line.split(":", 1)[1])

            elif line.startswith("OP0:"):
                inst["op0"] = parse_kv_list(line.split(":", 1)[1])

            elif line.startswith("OP1:"):
                inst["op1"] = parse_kv_list(line.split(":", 1)[1])

        if "op0" in inst and "op1" in inst:
            instruments.append(inst)

    return instruments

def emit_c_struct(instruments, label):
    print(f"\n// {label}")
    for inst in instruments:
        name = inst.get("name", f"Inst {inst['index']}")

        m = op_bytes(inst["op0"])   # modulator
        c = op_bytes(inst["op1"])   # carrier
        fb = (inst["fbconn"]["FB1"] << 1) | inst["fbconn"]["CONN1"]

        print(
            f"    [{inst['index']}] = {{ "
            f".m_ave=0x{m[0]:02X}, .m_ksl=0x{m[1]:02X}, .m_atdec=0x{m[2]:02X}, "
            f".m_susrel=0x{m[3]:02X}, .m_wave=0x{m[4]:02X}, "
            f".c_ave=0x{c[0]:02X}, .c_ksl=0x{c[1]:02X}, .c_atdec=0x{c[2]:02X}, "
            f".c_susrel=0x{c[3]:02X}, .c_wave=0x{c[4]:02X}, "
            f".feedback=0x{fb:02X} }}, // {name}"
        )

# --------------------------
# Usage
# --------------------------

with open("dmx-Bobby-Prince-v1.woplx", "r", encoding="utf-8") as f:
    text = f.read()

melodic_text = extract_section(text, "MELODIC_BANK:", "MELODIC_BANK_END")
perc_text    = extract_section(text, "PERCUSSION_BANK:", "PERCUSSION_BANK_END")

melodic = parse_instruments(melodic_text)
percussion = parse_instruments(perc_text)

emit_c_struct(melodic, "Melodic Bank")
emit_c_struct(percussion, "Percussion Bank")
