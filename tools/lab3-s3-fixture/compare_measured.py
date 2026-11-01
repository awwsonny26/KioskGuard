#!/usr/bin/env python3
from pathlib import Path
import json,re,statistics
from vcd import read_vcd,uart
from trace_metrics import lines,pulses
r=Path(__file__).resolve().parent; old=r.parent/'poll-suite-abf2-20261030/suite-incomplete.vcd';new=r/'suite-run-01/suite.vcd'
def summarize(path,scenario):
 tr=read_vcd(path);tx=lines(uart(tr['D2']));
 if scenario=='S1':
  boundaries={int(m[1]):x['start_ns'] for x in tx if (m:=re.match(r'SAMPLE t=(\d+)',x['text'])) and int(m[1]) in (15500,135500)}; start,end=boundaries[15500],boundaries[135500]
 else:
  q=json.loads((r/'suite-run-01/vcd-review.json').read_text())['pairs'];start=min(x['window_start_ns'] for x in q);end=max(x['window_end_ns'] for x in q)
 out={'start_ns':start,'end_ns':end}
 for name,ch,nominal in [('IMU','D0',20000),('ADC','D5',50000)]:
  ps=[(a,b) for a,b in pulses(tr[ch]) if start<=a and b<=end];width=[(b-a)/1000 for a,b in ps];period=[(y[0]-x[0])/1000 for x,y in zip(ps,ps[1:])]; jit=[abs(x-nominal) for x in period]
  def stat(a):return {'n':len(a),'min_us':min(a),'avg_us':statistics.mean(a),'max_us':max(a),'p99_us':sorted(a)[min(len(a)-1,int(len(a)*.99))]} if a else {'n':0}
  out[name]={'width':stat(width),'period':stat(period),'abs_period_deviation':stat(jit),'intervals_over_1_5_nominal':sum(v>1.5*nominal for v in period),'limitation':'GPIO interval is elapsed wall time in simulation, includes waits/interruptions; period deviation is not exact scheduler jitter/missed-release counter.'}
 return out
x={z:summarize(p,z) for z,p in [('S1',old),('S3',new)]};(r/'suite-run-01/comparison.json').write_text(json.dumps(x,indent=2)+'\n');print(json.dumps(x,indent=2))
