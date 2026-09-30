import json
import sys
import os

def extract_fn(addr_str):
    if addr_str.startswith("0x"):
        addr_int = int(addr_str, 16)
    else:
        addr_int = int(addr_str, 16)
    addr_norm = hex(addr_int)

    index_path = "halo_decompiled/index.jsonl"
    c_path = "halo_decompiled/cachebeta.elf.c"
    
    with open(index_path, "r") as f:
        for line in f:
            entry = json.loads(line)
            if entry["addr"] == addr_norm:
                offset = entry["c_offset"]
                length = entry["c_len"]
                with open(c_path, "rb") as cf:
                    cf.seek(offset)
                    data = cf.read(length).decode("utf-8", errors="replace")
                print(f"=== {entry['name']} @ {entry['addr']} (size {entry['size']} bytes) ===")
                print(data)
                return entry
    print(f"Function {addr_str} ({addr_norm}) not found in index.")
    return None

if __name__ == "__main__":
    if len(sys.argv) > 1:
        for a in sys.argv[1:]:
            extract_fn(a)
    else:
        print("Usage: extract_fn.py <addr> [...]")
