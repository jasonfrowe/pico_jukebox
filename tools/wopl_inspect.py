import sys

def inspect_wopl(filename):
    print(f"Inspecting {filename}...")
    with open(filename, "rb") as f:
        data = f.read()

    # Based on your previous successful scan:
    # Start Offset was 83. Stride was 66.
    start = 83
    stride = 66
    
    print("\n--- Instrument 0 (Raw Hex) ---")
    chunk = data[start : start+stride]
    
    # Print offsets and values
    print("Offset | Hex  | ASCII")
    print("-------|------|------")
    for i, b in enumerate(chunk):
        char = chr(b) if 32 <= b <= 126 else '.'
        print(f" +{i:<4} | 0x{b:02X} | {char}")

if __name__ == "__main__":
    inspect_wopl("Apogee-IMF-90.wopl")