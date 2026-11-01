#!/usr/bin/env python3
"""Measurements from KioskGuard LR3's own VCD. No waveform generation."""
import argparse
import bisect
import hashlib
import json
from pathlib import Path
import statistics
from vcd import read_vcd, uart

def summary(values):
    values = sorted(values)
    if not values:
        return {"n": 0}
    return {"n": len(values), "min_us": values[0], "avg_us": statistics.mean(values),
            "max_us": values[-1], "p99_us": values[min(len(values)-1, int(len(values)*.99))]}

def pulses(trace):
    out = []
    start = None
    for t, v in trace:
        if v:
            start = t
        elif start is not None:
            out.append((start, t)); start = None
    return out

def lines(data):
    out, current = [], []
    start = last = 0
    for t, value in data:
        if not current:
            start = t
        last = t
        if value in (10, 13):
            if current:
                out.append({"start_ns": start, "end_ns": last+round(10e9/115200),
                            "text": bytes(current).decode("ascii", errors="replace")})
            current = []
        else:
            current.append(value)
    return out

def analyze(path):
    traces = read_vcd(path)
    tx = lines(uart(traces.get("D2", [])))
    rx = lines(uart(traces.get("D3", [])))
    result = {"source": str(path.resolve()), "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
              "source_kind": "Wokwi VCD only; not physical hardware", "tasks": {}}
    for name, channel in (("imu", "D0"), ("audit", "D1"), ("adc", "D5")):
        spans = pulses(traces.get(channel, []))
        result["tasks"][name] = {"width": summary([(b-a)/1000 for a,b in spans]),
                                  "period": summary([(b[0]-a[0])/1000 for a,b in zip(spans,spans[1:])])}
    near = [t for t,v in traces.get("D4", []) if v]
    response = [t for t,v in traces.get("D7", []) if v]
    delays = []
    for t in near:
        i = bisect.bisect_left(response, t)
        if i < len(response):
            delays.append((response[i]-t)/1000)
    result["pir_to_policy_marker"] = summary(delays)
    result["pir_unpaired"] = len(near)-len(delays)
    result["uart_tx"], result["uart_rx"] = tx, rx
    # Matching is ordered by explicit mode replies; queue loss is not hidden.
    replies = [r for r in tx if "type=SERVICE_" in r["text"]]
    commands = [r for r in rx if r["text"] in ("service on", "service off")]
    latency, cursor = [], 0
    for command in commands:
        expected = "type=SERVICE_ON" if command["text"].endswith("on") else "type=SERVICE_OFF"
        while cursor < len(replies) and (replies[cursor]["start_ns"] < command["end_ns"] or expected not in replies[cursor]["text"]):
            cursor += 1
        if cursor < len(replies):
            latency.append((replies[cursor]["end_ns"]-command["end_ns"])/1000)
            cursor += 1
    result["service_uart_complete_ack"] = summary(latency)
    result["service_commands"] = len(commands)
    result["service_unpaired"] = len(commands)-len(latency)
    result["service_pairing_limit"] = "Ordered mode matching assumes no lost/reordered commands; check RX/queue loss counters and unmatched events before interpreting latency."
    return result

if __name__ == "__main__":
    parser=argparse.ArgumentParser();parser.add_argument("vcd", type=Path)
    parser.add_argument("--out", required=True, type=Path);args=parser.parse_args()
    args.out.write_text(json.dumps(analyze(args.vcd), ensure_ascii=False, indent=2))
