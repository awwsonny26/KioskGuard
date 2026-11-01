#!/usr/bin/env python3
"""Verify actual D4/D7 and complete export windows from Wokwi VCD."""
import argparse,bisect,hashlib,json,re
from pathlib import Path
from vcd import read_vcd,uart
from trace_metrics import lines
root=Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('phase',choices=['gate','suite']);p.add_argument('vcd',type=Path);a=p.parse_args()
expected=root/('workspace/docs/captures/gate.vcd' if a.phase=='gate' else 'workspace-suite/docs/captures/suite.vcd')
if a.vcd.resolve()!=expected.resolve():raise SystemExit('unexpected VCD path')
run=root/('gate-run-01' if a.phase=='gate' else 'suite-run-01'); summary=json.loads((run/'result.json').read_text()); tr=read_vcd(a.vcd); tx=lines(uart(tr.get('D2',[])));rx=lines(uart(tr.get('D3',[])))
boot=[x['text'] for x in tx if x['text'].startswith('BOOT ')]; rises=[t for t,v in tr.get('D4',[]) if v]; falls=[t for t,v in tr.get('D4',[]) if not v]; markers=[t for t,v in tr.get('D7',[]) if v]; events=[x for x in tx if 'type=USER_NEAR' in x['text']]; windows=[]; start=None
for row in tx:
 if row['text'].startswith('EXPORT slot=0 '):start=row['start_ns']
 elif row['text'].startswith('EXPORT slot=63 ') and start is not None:windows.append((start,row['end_ns']));start=None
pairs=[]
for i,t in enumerate(rises):
 next_rise=rises[i+1] if i+1<len(rises) else 1<<63
 win=next(((b,e) for b,e in windows if b<=t<=e),None)
 overlap_rx=[x for x in rx if win and win[0]<=x['start_ns'] and x['end_ns']<=win[1] and x['text'] in ('service on','service off')]
 j=bisect.bisect_left(markers,t); marker=markers[j] if j<len(markers) and markers[j]<next_rise and markers[j]-t<=1000000000 else None
 event_matches=[x for x in events if t<=x['start_ns']<next_rise];event=event_matches[0] if len(event_matches)==1 and 'severity=INFO' in event_matches[0]['text'] else None
 f=next((x for x in falls if x>t),None)
 pairs.append({'input_ns':t,'window_start_ns':win[0] if win else None,'window_end_ns':win[1] if win else None,'overlap_uart_commands':len(overlap_rx),'marker_ns':marker,'event_count':len(event_matches),'event_end_ns':event['end_ns'] if event else None,'hold_ns':f-t if f else None,'low_before_ns':t-max((x for x in falls if x<t),default=0),'policy_latency_us':(marker-t)/1000 if marker else None,'full_tx_latency_us':(event['end_ns']-t)/1000 if event else None})
valid_pairs=[q for q in pairs if q['window_start_ns'] is not None and q['overlap_uart_commands']>0 and q['marker_ns'] is not None and q['event_end_ns'] is not None and q['hold_ns'] is not None and 4900000000<=q['hold_ns']<=5100000000 and q['low_before_ns']>=1200000000]
need=3 if a.phase=='gate' else 30
workspace=root/'workspace'; inputs={k:hashlib.sha256((workspace/p).read_bytes()).hexdigest() for k,p in {'elf':'firmware/build-poll/firmware.elf','fixture_wasm':'chips/pirfixture.chip.wasm','diagram':'diagram.json'}.items()}
diag=[x['text'] for x in tx if x['text'].startswith('RXDIAG ')]
clean_diag=bool(diag) and all(re.search(r'(?:^| )'+key+r'=0(?: |$)',diag[-1]) for key in ('errors','drop','armfail'))
valid=summary.get('state')=='captured_pending_VCD_review' and len(boot)==1 and 'candidate-poll-tree-abf27f5853950696' in boot[0] and len(valid_pairs)>=need and len({q['window_start_ns'] for q in valid_pairs})>=need and clean_diag
result={'valid':valid,'phase':a.phase,'vcd_sha256':hashlib.sha256(a.vcd.read_bytes()).hexdigest(),'vcd_path':str(a.vcd.resolve()),'inputs':inputs,'boot':boot,'required_pairs':need,'valid_pair_count':len(valid_pairs),'input_count':len(rises),'event_count':len(events),'complete_export_windows':len(windows),'pairs':pairs,'rxdiag':diag,'clean_rxdiag':clean_diag,'rx_commands':len(rx),'limitations':'Synthetic GPIO substitutes PIR OUT; validate all 5s high and 1.2s low, event ordering, loss counters and actual operation window; simulated hardware only.'}
(run/'vcd-review.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
if a.phase=='gate':(root/'gate-proof.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'valid':valid,'valid_pairs':len(valid_pairs),'input_count':len(rises),'events':len(events),'windows':len(windows)}))
raise SystemExit(0 if valid else 1)
