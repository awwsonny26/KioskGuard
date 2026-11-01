#!/usr/bin/env python3
"""Bounded real Wokwi UART acquisition; gate and suite are separate runs."""
import argparse,hashlib,json,re,time
from pathlib import Path
from datetime import datetime,timezone
import serial
root=Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('phase',choices=['gate','suite']);a=p.parse_args()
vcd=root/('workspace/docs/captures/gate.vcd' if a.phase=='gate' else 'workspace-suite/docs/captures/suite.vcd')
if vcd.exists():raise SystemExit('existing VCD: choose a fresh run and preserve prior file')
if a.phase=='gate' and (root/'gate-proof.json').exists():raise SystemExit('existing gate proof: choose fresh run')
if a.phase=='suite':
 proof=root/'gate-proof.json'
 if not proof.exists(): raise SystemExit('gate VCD proof required')
 data=json.loads(proof.read_text())
 if data.get('valid') is not True: raise SystemExit('gate proof failed')
 gate_vcd=root/'workspace/docs/captures/gate.vcd'
 if not gate_vcd.exists() or hashlib.sha256(gate_vcd.read_bytes()).hexdigest()!=data.get('vcd_sha256'):raise SystemExit('gate VCD missing or changed')
 for key in ('elf','fixture_wasm','diagram'):
  path=root/'workspace-suite'/{'elf':'firmware/build-poll/firmware.elf','fixture_wasm':'chips/pirfixture.chip.wasm','diagram':'diagram.json'}[key]
  if hashlib.sha256(path.read_bytes()).hexdigest()!=data['inputs'][key]: raise SystemExit('gate/suite input mismatch: '+key)
out=root/('gate-run-01' if a.phase=='gate' else 'suite-run-01');out.mkdir(exist_ok=False)
start=time.monotonic(); latest=None; first=None; last_host=start; seen={'near':0,'export_0':0,'export_63':0}; state={'state':'incomplete','reason':'running'}
def stamp():return {'host_utc':datetime.now(timezone.utc).isoformat(),'host_monotonic_ns':time.monotonic_ns()}
def finish(): (out/'result.json').write_text(json.dumps(state,ensure_ascii=False,indent=2)+'\n')
finish()
try:
 with serial.serial_for_url('rfc2217://127.0.0.1:14379',baudrate=115200,timeout=.1) as port,(out/'uart.raw').open('wb',buffering=0) as raw,(out/'events.jsonl').open('w',buffering=1) as ev:
  def record(kind,**kw): ev.write(json.dumps({'kind':kind,**stamp(),**kw},ensure_ascii=False)+'\n')
  def send(cmd,planned):port.write((cmd+'\n').encode());record('tx',command=cmd,planned_offset_ms=planned,latest_sample_ms=latest)
  record('connected'); buf=b''; actions=[]; stress=[]; count=3 if a.phase=='gate' else 45
  # At least 8.5s between export commands; each input pulse is 5s high + 1.2s low.
  actions += [(3000+i*8500,'export') for i in range(count)]
  actions += [(1000,'stats'),(2000,'rxdiag')]
  for t in range(5000,3000+count*8500,2000):actions.append((t,'service on' if (t//2000)%2 else 'service off'))
  if a.phase=='suite':
   end_export=3000+count*8500
   actions += [(end_export+2000,'fault on'),(end_export+9000,'fault off'),(end_export+16000,'rate 20'),(end_export+26000,'rate 10'),(end_export+36000,'rate 5'),(end_export+46000,'rate 20'),(end_export+48000,'stats'),(end_export+49000,'rxdiag'),(end_export+50000,'audit status')]
   end_ms=end_export+57000
  else: actions += [(3000+count*8500,'rxdiag'),(4000+count*8500,'audit status')];end_ms=10000+count*8500
  actions.sort(); idx=0
  while True:
   data=port.read(4096)
   if data:
    off=raw.tell();raw.write(data);record('rx',offset=off,length=len(data),latest_sample_ms=latest);buf+=data
    lines=buf.split(b'\n');buf=lines.pop()
    for line in lines:
     s=line.decode('utf8','replace').rstrip('\r')
     if s.startswith('BOOT') and first is not None: raise RuntimeError('unexpected BOOT/reset')
     if s.startswith('EXPORT slot=0 '):
      seen['export_0']+=1
      anchor=time.monotonic();stress.extend([(anchor+.04,'service on'),(anchor+.16,'service off')])
     if s.startswith('EXPORT slot=63 '):seen['export_63']+=1
     if 'type=USER_NEAR' in s:seen['near']+=1
     m=re.match(r'^SAMPLE t=(\d+)\b',s)
     if m:
      now=time.monotonic(); t=int(m[1]);
      if latest is not None and (t<=latest or t-latest>8000 or now-last_host>20):raise RuntimeError('sample reset/gap/stall')
      latest=t;last_host=now
      if first is None:first=t;record('start',first_mcu_ms=t)
   due=[item for item in stress if item[0]<=time.monotonic()];stress=[item for item in stress if item[0]>time.monotonic()]
   for _,cmd in due:send(cmd,None)
   if time.monotonic()-last_host>20:raise RuntimeError('SAMPLE timeout')
   if time.monotonic()-start>(end_ms/1000)*3+120:raise RuntimeError('overall host timeout')
   if first is None:continue
   while idx<len(actions) and latest-first>=actions[idx][0]:
    t,cmd=actions[idx];idx+=1;send(cmd,t)
   if latest-first>=end_ms:
    state={'state':'captured_pending_VCD_review','phase':a.phase,'first_mcu_ms':first,'last_mcu_ms':latest,'seen':seen,'sent_actions':idx,'limitations':'UART collection is not VCD overlap or hardware proof; examine GPIO and RX loss'};record('end',**state);break
except Exception as e:state={'state':'incomplete','reason':str(e),'first_mcu_ms':first,'last_mcu_ms':latest,'seen':seen}
finally:finish()
