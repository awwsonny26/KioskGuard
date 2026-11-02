"""Host-only synthetic unit tests; these are not MCU/VCD evidence."""
import json,runpy,shutil,sys,tempfile,types
from pathlib import Path
src=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='kg-validator-') as tmp:
 r=Path(tmp);shutil.copy(src/'verify_vcd.py',r/'verify_vcd.py');shutil.copytree(src/'workspace',r/'workspace');(r/'gate-run-01').mkdir();(r/'gate-run-01/result.json').write_text(json.dumps({'state':'captured_pending_VCD_review'}));path=r/'workspace/docs/captures/gate.vcd';path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(b'host mock only')
 tx=[{'text':'BOOT project=KioskGuard source=candidate-spans-20261101','start_ns':0,'end_ns':1000},{'text':'RXDIAG errors=0 drop=0 armfail=0','start_ns':1001,'end_ns':1002}]; d4=[(0,0)];d7=[]
 for i in range(3):
  t=10000000000+i*8500000000; d4.extend([(t,1),(t+5000000000,0)]);d7.extend([(t+10000000,1),(t+10000100,0)]);tx.extend([{'text':'EXPORT slot=0 ok=1','start_ns':t-100000000,'end_ns':t-90000000},{'text':'EVENT type=USER_NEAR severity=INFO','start_ns':t+20000000,'end_ns':t+30000000},{'text':'EXPORT slot=63 ok=1','start_ns':t+800000000,'end_ns':t+900000000}])
 tx.sort(key=lambda x:x['start_ns']);rc=[{'text':'service on','start_ns':10000000000+i*8500000000+40000000,'end_ns':10000000000+i*8500000000+41000000} for i in range(3)];tr={'D2':tx,'D3':rc,'D4':d4,'D7':d7}
 sys.modules['vcd']=types.SimpleNamespace(read_vcd=lambda _:tr,uart=lambda x:x);sys.modules['trace_metrics']=types.SimpleNamespace(lines=lambda x:x)
 sys.argv=['verify_vcd.py','gate',str(path)]
 try:runpy.run_path(str(r/'verify_vcd.py'),run_name='__main__')
 except SystemExit as e:assert e.code==0
 else:raise AssertionError('expected exit')
 assert json.loads((r/'gate-proof.json').read_text())['valid_pair_count']==3
 # Shift a real input before its operation; it must not count.
 d4[1]=(d4[1][0]-200000000,1)
 try:runpy.run_path(str(r/'verify_vcd.py'),run_name='__main__')
 except SystemExit as e:assert e.code==1
 else:raise AssertionError('expected failed gate')
print('PASS host synthetic positive/negative overlap logic; no MCU evidence')
