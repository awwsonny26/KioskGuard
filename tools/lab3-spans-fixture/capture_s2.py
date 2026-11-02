#!/usr/bin/env python3
"""Continuous real Wokwi S2; UART capture alone is not hardware evidence."""
import hashlib
import json
import re
import time
from datetime import datetime, timezone
from pathlib import Path
import serial

out = Path(__file__).resolve().parent / 's2-run-01'
out.mkdir(exist_ok=False)
result = {'state': 'incomplete', 'reason': 'running'}
first = last = base = None
gate_flags = set()
phase = 'gate'
gate_actions = [(1000, 'stats'), (3000, 'service on'), (5000, 'service off'), (7000, 'rxdiag')]
actions = [(1000, 'stats'), (2000, 'audit status')]
for t in range(300000, 3600000, 300000):
    actions.extend([(t, 'stats'), (t + 2000, 'audit status')])
for t in range(30000, 3600000, 600000):
    actions.extend([(t, 'service on'), (t + 2000, 'service off')])
actions.sort()
gate_index = action_index = 0
last_host = time.monotonic()
end_seen = set()

def stamp():
    return {'utc': datetime.now(timezone.utc).isoformat(), 'monotonic_ns': time.monotonic_ns()}

def save():
    (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n')

save()
try:
    with serial.serial_for_url('rfc2217://127.0.0.1:14379', baudrate=115200, timeout=.1) as port, \
         (out / 'uart.raw').open('wb', buffering=0) as raw, \
         (out / 'events.jsonl').open('w', buffering=1) as events:
        def emit(kind, **kw):
            events.write(json.dumps({'kind': kind, **stamp(), **kw}) + '\n')
        def send(cmd, planned):
            port.write((cmd + '\n').encode())
            emit('tx', command=cmd, latest_sample_ms=last, planned_offset_ms=planned)
        emit('connected')
        buffer = b''
        while True:
            data = port.read(4096)
            if data:
                offset = raw.tell()
                raw.write(data)
                emit('rx', offset=offset, length=len(data), latest_sample_ms=last)
                buffer += data
                lines = buffer.split(b'\n')
                buffer = lines.pop()
                for line in lines:
                    text = line.decode('utf8', 'replace').rstrip('\r')
                    if text.startswith('BOOT') and last is not None:
                        raise RuntimeError('BOOT/reset during capture')
                    if phase == 'gate':
                        if text.startswith('SYSTEM '): gate_flags.add('stats')
                        if 'type=SERVICE_ON' in text: gate_flags.add('on')
                        if 'type=SERVICE_OFF' in text: gate_flags.add('off')
                        if text.startswith('RXDIAG ') and all(k+'=0' in text for k in ('errors', 'drop', 'armfail')):
                            gate_flags.add('rxdiag')
                    if phase == 'end':
                        for tag in ('SYSTEM', 'AUDIT', 'RXDIAG'):
                            if text.startswith(tag + ' '): end_seen.add(tag)
                    match = re.match(r'^SAMPLE t=(\d+)\b', text)
                    if match:
                        now = time.monotonic()
                        t = int(match[1])
                        if last is not None and (t <= last or t-last > 5000 or now-last_host > 15):
                            raise RuntimeError('SAMPLE reset, gap or host stall')
                        last, last_host = t, now
                        if first is None: first = t
            if time.monotonic()-last_host > 15: raise RuntimeError('SAMPLE timeout')
            if last is None: continue
            if phase == 'gate':
                while gate_index < len(gate_actions) and last-first >= gate_actions[gate_index][0]:
                    t, cmd = gate_actions[gate_index]; gate_index += 1; send(cmd, t)
                if last-first >= 15000 and gate_flags == {'stats', 'on', 'off', 'rxdiag'}:
                    base = last; phase = 's2'
                    emit('start', first_mcu_ms=base, gate_checks=sorted(gate_flags))
                elif last-first >= 30000:
                    raise RuntimeError('gate checks failed: ' + str(sorted(gate_flags)))
            elif phase == 's2':
                while action_index < len(actions) and last-base >= actions[action_index][0]:
                    t, cmd = actions[action_index]; action_index += 1; send(cmd, t)
                if last-base >= 3600000:
                    result.update(first_mcu_ms=base, last_mcu_ms=last, elapsed_mcu_ms=last-base)
                    emit('elapsed', **result)
                    for cmd in ('stats', 'audit status', 'rxdiag'): send(cmd, 3600000)
                    phase = 'end'; end_host = time.monotonic()
            elif end_seen == {'SYSTEM', 'AUDIT', 'RXDIAG'}:
                result.update(state='transport_complete_pending_vcd_review', reason=None, end_seen=sorted(end_seen))
                break
            elif time.monotonic()-end_host > 15:
                raise RuntimeError('end responses missing: ' + str(sorted(end_seen)))
except Exception as exc:
    result.update(state='incomplete', reason=str(exc), first_mcu_ms=base, last_mcu_ms=last)
finally:
    for name in ('uart.raw', 'events.jsonl'):
        f = out / name
        if f.exists(): result[name+'_sha256'] = hashlib.sha256(f.read_bytes()).hexdigest()
    result['finish'] = stamp()
    save()
