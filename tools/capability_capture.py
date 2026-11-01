#!/usr/bin/env python3
"""Capture the isolated capability image after an authorized GUI start.

This tool neither starts nor stops Wokwi. Host receipt times are transport data.
The full BOOT identity and signal timings must also be checked in the saved VCD.
"""
import argparse
import hashlib
import json
from pathlib import Path
import time


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--url", default="rfc2217://127.0.0.1:14379")
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    import serial
    args.out.mkdir(parents=True, exist_ok=False)
    start = time.monotonic()
    sent = done = False
    actions = []
    buffer = b""
    with serial.serial_for_url(args.url, baudrate=115200, timeout=0.05) as port:
        with (args.out / "uart.log").open("wb") as log:
            while time.monotonic() - start < 60:
                data = port.read(4096)
                if not data:
                    continue
                log.write(data)
                log.flush()
                buffer += data
                parts = buffer.split(b"\n")
                buffer = parts.pop()
                for line in parts:
                    if not sent and (b"PROBE_UART" in line or b"PROBE_TICK" in line):
                        stimulus = b"kg-pilot-35\n"
                        port.write(stimulus)
                        actions.append({"host_elapsed_s": time.monotonic()-start,
                                        "bytes_hex": stimulus.hex(),
                                        "clock_domain": "host transport only"})
                        sent = True
                    if b"PROBE_DONE" in line:
                        done = True
                if done:
                    break
    raw = args.out / "uart.log"
    manifest = {"status": "probe_done_seen" if done else "incomplete_timeout",
                "host_elapsed_s": time.monotonic()-start,
                "source_kind": "actual RFC2217 received bytes",
                "uart_sha256": hashlib.sha256(raw.read_bytes()).hexdigest(),
                "actions": actions,
                "limits": "No application S1/S2/S3. Check actual BOOT and VCD separately."}
    (args.out / "transport.json").write_text(json.dumps(manifest, indent=2)+"\n")
    print(json.dumps(manifest))


if __name__ == "__main__":
    main()
