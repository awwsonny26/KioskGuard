"""Host-only: changed initial Flash bytes must prevent the suite from starting."""
import hashlib,json,runpy,shutil,sys,tempfile,types
from pathlib import Path
source=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='kg-input-guard-') as tmp:
 root=Path(tmp);shutil.copy(source/'run_suite.py',root/'run_suite.py')
 paths={'elf':'firmware/build-poll/firmware.elf','hex':'firmware/build-poll/firmware.hex',
        'fixture_wasm':'chips/pirfixture.chip.wasm','diagram':'diagram.json'}
 for workspace in ('workspace','workspace-suite'):
  for path in paths.values():
   p=root/workspace/path;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(path.encode())
 vcd=root/'workspace/docs/captures/gate.vcd';vcd.parent.mkdir(parents=True);vcd.write_bytes(b'host mock only')
 proof={'valid':True,'vcd_sha256':hashlib.sha256(vcd.read_bytes()).hexdigest(),
        'inputs':{key:hashlib.sha256((root/'workspace'/path).read_bytes()).hexdigest() for key,path in paths.items()}}
 (root/'gate-proof.json').write_text(json.dumps(proof));(root/'workspace-suite'/paths['hex']).write_bytes(b'changed initial flash')
 sys.modules['serial']=types.SimpleNamespace(serial_for_url=lambda *a,**kw:(_ for _ in ()).throw(AssertionError('must not connect')))
 sys.argv=['run_suite.py','suite']
 try:runpy.run_path(str(root/'run_suite.py'),run_name='__main__')
 except SystemExit as exc:assert 'input mismatch: hex' in str(exc)
 else:raise AssertionError('changed HEX was accepted')
print('PASS host-only: mismatched HEX blocked before simulator connection')
