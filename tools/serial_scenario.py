#!/usr/bin/env python3
"""Drive the project's RFC2217 UART after the simulator has been started.

Host receipt times describe transport only. Performance comes from MCU fields/VCD.
The one-hour scenario stops on MCU elapsed milliseconds, not a fast host loop.
"""
import argparse
import hashlib
import json
from pathlib import Path
import random
import re
import sys
import time

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("scenario", choices=("S1", "S2", "S3", "fault", "boundary"))
    parser.add_argument("--url", default="rfc2217://127.0.0.1:14379")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--minutes", type=int, default=60)
    args = parser.parse_args()
    import serial
    args.out.mkdir(parents=True, exist_ok=False)
    raw = args.out / "uart.log"
    root = Path(__file__).resolve().parents[1]
    run_info = {"state": "started_incomplete_until_transport_json_exists",
                "argv": sys.argv, "scenario": args.scenario, "url": args.url,
                "runner_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                "expected_source_banner": (root/"firmware/Platform/version.h").read_text().strip(),
                "expected_firmware_manifest": json.loads((root/"docs/metrics/build-manifest.json").read_text()),
                "identity_limit": "Expected local build; verify against an actual MCU BOOT record before accepting results."}
    (args.out/"run.json").write_text(json.dumps(run_info, ensure_ascii=False, indent=2))
    transport = []
    start_wall = time.monotonic()
    end_ms = {"S1": 120000, "S2": max(60, args.minutes)*60000,
              "S3": 120000, "fault": 20000, "boundary": 90000}[args.scenario]
    # All actions are relative to the first received MCU timestamp.
    actions = [(1000, "stats"), (2000, "audit status")]
    if args.scenario == "S2":
        for t in range(10000, end_ms, 60000):
            actions.extend(((t, "service on"), (t+1000, "service off")))
        for t in range(300000, end_ms, 300000):
            actions.extend(((t, "stats"), (t+1000, "audit status")))
    elif args.scenario == "S3":
        rng = random.Random(35031)
        for i in range(40):
            t = 5000 + i*2200 + rng.randrange(1000)
            actions.append((t, "export"))
        actions += [(95000, "stats"), (97000, "audit status")]
    elif args.scenario == "fault":
        actions += [(3000, "fault on"), (5000, "service on"),
                    (9000, "fault off"), (12000, "service off"), (15000, "stats")]
    elif args.scenario == "boundary":
        for base, rate in ((3000, 20), (25000, 10), (47000, 5)):
            actions.append((base, f"rate {rate}"))
            for i in range(30):
                actions.append((base+1000+i*500, "service on\nservice off\n"*4))
            actions.append((base+17000, "stats"))
        actions.append((75000, "rate 20"))
    actions += [(end_ms-3000, "stats"), (end_ms-1000, "audit status")]
    actions.sort()
    first = latest = None
    index = 0
    pending_host = []
    interleaving_index = 0
    # The driver never launches or stops the simulator; GUI ownership stays external.
    timeout = 0.01 if args.scenario == "S3" else 0.25
    with serial.serial_for_url(args.url, baudrate=115200, timeout=timeout) as port, raw.open("wb") as log, (args.out/"actions.jsonl").open("w", buffering=1) as action_log:
        def record(action):
            transport.append(action)
            action_log.write(json.dumps(action, ensure_ascii=False)+"\n")
        buffer = b""
        while first is None or latest-first < end_ms:
            if time.monotonic()-start_wall > max(600, end_ms/1000*4):
                raise TimeoutError("MCU timestamp did not reach scenario duration; run is incomplete")
            data = port.read(4096)
            if data:
                log.write(data); log.flush()
                buffer += data
                lines = buffer.split(b"\n"); buffer = lines.pop()
                for line in lines:
                    m = re.search(rb"(?:^| )t=(\d+)", line)
                    if m:
                        latest = int(m.group(1))
                        if first is None:
                            first = latest
            if first is None:
                continue
            while index < len(actions) and actions[index][0] <= latest-first:
                target, command = actions[index]; index += 1
                port.write((command+"\n").encode())
                record({"requested_mcu_offset_ms": target,
                                  "latest_received_mcu_ms": latest,
                                  "host_elapsed_s": time.monotonic()-start_wall,
                                  "command": command})
                if args.scenario == "S3" and command == "export":
                    # EXPORT can starve SAMPLE/tick text. An asynchronous input
                    # must not wait for a new MCU log line from the busy output.
                    anchor = time.monotonic()
                    mode = "service on" if interleaving_index%2 == 0 else "service off"
                    pending_host.extend(((anchor+0.017, "audit save", target),
                                         (anchor+0.033, mode, target)))
                    interleaving_index += 1
            host_now = time.monotonic()
            due = [action for action in pending_host if action[0] <= host_now]
            pending_host = [action for action in pending_host if action[0] > host_now]
            for requested_host, command, export_mcu_offset in due:
                port.write((command+"\n").encode())
                record({"after_export_mcu_offset_ms": export_mcu_offset,
                                  "requested_host_elapsed_s": requested_host-start_wall,
                                  "host_elapsed_s": time.monotonic()-start_wall,
                                  "latest_received_mcu_ms": latest,
                                  "clock_domain": "host stimulus scheduling only; validate overlap in VCD",
                                  "command": command})
    manifest = {"scenario": args.scenario, "status": "transport_completed",
                "first_mcu_ms": first, "last_mcu_ms": latest,
                "elapsed_mcu_ms": latest-first, "host_elapsed_s": time.monotonic()-start_wall,
                "uart_sha256": hashlib.sha256(raw.read_bytes()).hexdigest(),
                "actions": transport,
                "limitations": "Receipt timestamps are not MCU latency. Validate boot hash, STAT counts, VCD, failures and FR separately."}
    (args.out / "transport.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2))

if __name__ == "__main__":
    main()
