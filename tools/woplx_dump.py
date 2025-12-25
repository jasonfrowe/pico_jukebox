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
    reg20 = (op["AM"] << 7) | (op["VB"] << 6) | (op["EG"] << 5) | (op["KR"] << 4) | op["ML"]
    reg40 = (op["KL"] << 6) | op["TL"]
    reg60 = (op["AT"] << 4) | op["DC"]
    reg80 = (op["ST"] << 4) | op["RL"]
    regE0 = op["WF"]
    return [reg20, reg40, reg60, reg80, regE0]

def extract_melodic_bank(text):
    lines = text.splitlines()
    in_bank = False
    bank_lines = []

    for line in lines:
        line = line.rstrip()

        if line.startswith("MELODIC_BANK:"):
            in_bank = True
            continue

        if line.startswith("MELODIC_BANK_END"):
            break

        if in_bank:
            bank_lines.append(line)

    return "\n".join(bank_lines)

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

def emit_patches(instruments):
    for inst in instruments:
        name = inst.get("name", f"Inst {inst['index']}")
        print(f"    # {inst['index']}: {name}")

        b0 = op_bytes(inst["op0"])
        b1 = op_bytes(inst["op1"])
        fb = (inst["fbconn"]["FB1"] << 1) | inst["fbconn"]["CONN1"]

        bytes_all = b1 + b0 + [fb]
        print("    " + ",".join(f"0x{b:02X}" for b in bytes_all) + ",")

# --------------------------
# Usage
# --------------------------

with open("Apogee-IMF-90.woplx", "r", encoding="utf-8") as f:
    text = f.read()

melodic_text = extract_melodic_bank(text)
instruments = parse_instruments(melodic_text)
emit_patches(instruments)
