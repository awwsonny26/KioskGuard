"""Synthetic UART logic test only; does not launch a simulator."""
import json,runpy,shutil,sys,tempfile,types
from pathlib import Path
class Port:
 def __init__(self):self.t=0;self.pending=[]
 def __enter__(self):return self
 def __exit__(self,*a):pass
 def write(self,b):
  s=b.decode().strip()
  if s=='export':self.pending+=['EXPORT slot=0 ok=1 data=aa','EVENT type=USER_NEAR severity=INFO','EXPORT slot=63 ok=1 data=aa']
  if s=='rxdiag':self.pending+=['RXDIAG errors=0 drop=0 armfail=0 line=test']
 def read(self,n):
  self.t+=500;d='\n'.join(self.pending+[f'SAMPLE t={self.t} imu=OK'])+'\n';self.pending=[];return d.encode()
with tempfile.TemporaryDirectory(prefix='kg-runner-') as tmp:
 p=Path(tmp);shutil.copy(Path(__file__).with_name('run_suite.py'),p/'run_suite.py');sys.modules['serial']=types.SimpleNamespace(serial_for_url=lambda *a,**kw:Port());sys.argv=['run_suite.py','gate'];runpy.run_path(str(p/'run_suite.py'),run_name='__main__');r=json.loads((p/'gate-run-01/result.json').read_text());assert r['state']=='captured_pending_VCD_review' and r['seen']['near']==3 and r['seen']['export_63']==3
print('PASS host synthetic gate transport; no MCU evidence')
