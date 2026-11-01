#!/usr/bin/env python3
"""Check CRC/commit of actual EXPORT UART lines; never synthesise flash content."""
import argparse
import json
from pathlib import Path
import re
import struct
import zlib

def decode(text):
    slots = {}
    for match in re.finditer(r"EXPORT slot=(\d+) ok=1 data=([0-9a-f]{128})", text):
        slots[int(match[1])] = bytes.fromhex(match[2])
    valid, invalid, erased = [], [], []
    for slot, data in sorted(slots.items()):
        if data == b"\xff"*64:
            erased.append(slot); continue
        fields = struct.unpack("<16I", data)
        if (fields[0] == 0x3341474b and fields[14:] == (0x434f4d33, 0xbcb0b2cc)
                and fields[13] == zlib.crc32(data[:52])):
            valid.append({"slot": slot, "generation": fields[1], "near": fields[2],
                          "shocks": fields[3], "moves": fields[4], "lights": fields[5],
                          "errors": fields[6], "services": fields[7]})
        else:
            invalid.append(slot)
    return {"received_slots": len(slots), "complete_export": set(slots) == set(range(64)),
            "valid": valid, "invalid": invalid, "erased": erased}

if __name__ == "__main__":
    p = argparse.ArgumentParser();p.add_argument("log", type=Path)
    a = p.parse_args();print(json.dumps(decode(a.log.read_text(errors="replace")), indent=2))
