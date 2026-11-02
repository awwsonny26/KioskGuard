from pathlib import Path
import time,serial,re,json,hashlib
root=Path(__file__).parent;start=time.monotonic();p=None
while time.monotonic()-start<180:
 try:p=serial.serial_for_url('rfc2217://127.0.0.1:14379',baudrate=115200,timeout=.1);break
 except (serial.SerialException,OSError):time.sleep(.05)
if not p:raise SystemExit('port unavailable')
buf=b'';first=last=None;n=0;gaps=[];sent=set();got=set();fail=None
with p,(root/'s1-uart.raw').open('xb',buffering=0) as out:
 while time.monotonic()-start<1200:
  x=p.read(4096)
  if x:
   out.write(x);buf+=x;ls=buf.split(b'\n');buf=ls.pop()
   for line in ls:
    if line.startswith(b'SYSTEM '):got.add('SYSTEM')
    if line.startswith(b'RXDIAG '):got.add('RXDIAG')
    m=re.match(rb'SAMPLE t=(\d+)',line)
    if m:
     t=int(m.group(1)); n+=1
     if first is None:first=t
     if last is not None and t-last!=500:gaps.append(t-last)
     if last is not None and t<=last:fail='sample reset';break
     last=t
  if fail:break
  if first is not None and last-first>=120000 and 'stats' not in sent:p.write(b'stats\n');sent.add('stats')
  if 'SYSTEM' in got and 'rxdiag' not in sent:p.write(b'rxdiag\naudit status\n');sent.add('rxdiag')
  if {'SYSTEM','RXDIAG'}<=got and last-first>=122000:break
r={'first_mcu_ms':first,'last_mcu_ms':last,'samples':n,'non_500_intervals':gaps,'got':list(got),'commands':list(sent),'error':fail,'state':'captured_pending_vcd' if first is not None and last-first>=120000 and not gaps and not fail and {'SYSTEM','RXDIAG'}<=got else 'incomplete'};(root/'s1-result.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r))
