#!/usr/bin/env python3
"""Post-process actual suite; observations are not automatic FR verdicts."""
import json,re,statistics
from pathlib import Path
from vcd import read_vcd,uart
from trace_metrics import lines
r=Path(__file__).resolve().parent; v=r/'workspace-suite/docs/captures/suite.vcd';o=r/'suite-run-01';tr=read_vcd(v);tx=lines(uart(tr.get('D2',[])));rx=lines(uart(tr.get('D3',[])))
commands=[x for x in rx if x['text'] in ('fault on','fault off','rate 20','rate 10','rate 5','stats','rxdiag')]
events=[x for x in tx if re.search(r'type=(SENSOR_ERROR|SENSOR_RECOVERED|DIAGNOSTIC)',x['text'])]
spans=[]
for i,c in enumerate(commands):
 if c['text'] not in ('rate 20','rate 10','rate 5','fault on','fault off'):continue
 end=commands[i+1]['start_ns'] if i+1<len(commands) else max((t for t,_ in tr['D0']),default=c['end_ns']);times=[t for t,val in tr['D0'] if val and c['end_ns']<=t<end];interval=[(b-a)/1000 for a,b in zip(times,times[1:])]
 spans.append({'command':c,'end_ns':end,'imu_rises':len(times),'period_min_us':min(interval) if interval else None,'period_avg_us':statistics.mean(interval) if interval else None,'period_max_us':max(interval) if interval else None})
result={'fault_kind':'software changes target I2C address; NACK injection, not unplug or physical bus short','commands':commands,'events':events,'periods':spans,'stats':[x for x in tx if x['text'].startswith(('RXDIAG ','AUDIT ','METRICS_UNAVAILABLE'))],'memory':'Runtime stack/heap are not exposed by candidate stats; static link values do not measure runtime peaks.','limitations':'Marker intervals contain waits/interrupts; compare actual thresholds and loss counters manually. Missing output is unavailable, not zero.'}
(o/'suite-observations.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');print('Wrote observational summary; no automatic pass')
