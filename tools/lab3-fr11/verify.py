from pathlib import Path
import hashlib,json,re,sys
import vcd
p=Path(sys.argv[1]); raw=Path(sys.argv[2]).read_bytes(); traces=vcd.read_vcd(p); tx=vcd.uart(traces['D2']);rx=vcd.uart(traces['D3']); packets=vcd.i2c(traces['D0'],traces['D1']); tnear=next(t for t,v in traces['D4'] if v);tshock=next(t for t,v in traces['D5'] if v)
lines=[];b=[]
for t,x in tx:
 if not b:start=t
 if x in (10,13):
  if b:lines.append((start,t+int(10*1e9/115200),bytes(b).decode('ascii','replace')));b=[]
 else:b.append(x)
def item(s):
 hits=[x for x in lines if s in x[2]]
 if len(hits)!=1:raise ValueError((s,len(hits)))
 return hits[0]
boot=item('BOOT project=KioskGuard lab=3');baseline=item('type=BASELINE_READY');near=item('type=USER_NEAR');shock=item('type=BODY_SHOCK')
assert 'candidate-poll-tree-abf27f5853950696' in boot[2]
assert abs(tshock-tnear)<1000 and baseline[0]<tnear
assert 'mode=CONTROL severity=INFO' in near[2] and 'mode=CONTROL severity=WARN' in shock[2]
assert 'ax=2000 ay=0 az=1000' in shock[2] and 'metric=4000000' in shock[2]
assert all(line[2].encode() in raw for line in (boot,baseline,near,shock))
prior=[x for x in packets if x['end']<tshock and x['bytes'][:7]==[209,0,0,0,0,32,0]]
after=[x for x in packets if x['end']>tshock and x['start']<tshock+20000000 and x['bytes'][:7]==[209,64,0,0,0,32,0]]
assert prior and after
lat_near=(near[1]-tnear)/1e6;lat_shock=(shock[1]-tshock)/1e6
assert 0<lat_near<200 and 0<lat_shock<200
out={'valid':True,'stimulus_ns':{'presence_d4':tnear,'shock_register_marker_d5':tshock},'i2c_before':prior[-1],'i2c_step':after[0],'events':{'shock':{'line':shock[2],'start_ns':shock[0],'end_ns':shock[1],'stimulus_to_end_ms':lat_shock},'presence':{'line':near[2],'start_ns':near[0],'end_ns':near[1],'stimulus_to_end_ms':lat_near}},'threshold':'vector delta > 1 g; stimulus delta 2 g, metric 4,000,000 mg²','scope':'one synthetic I2C MPU register model and independent synthetic GPIO, Wokwi only; physical sensors untested','vcd_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'raw_sha256':hashlib.sha256(raw).hexdigest()}
print(json.dumps(out,indent=2))
