#!/usr/bin/env python3
"""Linked symbol accounting. Run with ARM binutils from the build image."""
from pathlib import Path
import collections
import json
import re
import subprocess

ROOT=Path(__file__).resolve().parents[1]
ELF=ROOT/'firmware/build/firmware.elf'
def run(*args):return subprocess.check_output(args,text=True)

if __name__=='__main__':
    sections={}
    for line in run('arm-none-eabi-size','-A',str(ELF)).splitlines():
        match=re.match(r'^(\.[\w.]+)\s+(\d+)\s+(\d+)$',line)
        if match:sections[match[1]]={'bytes':int(match[2]),'address':int(match[3])}
    flash=sum(s['bytes'] for s in sections.values() if 0x08000000<=s['address']<0x08007000)+sections['.data']['bytes']
    ram=sum(s['bytes'] for s in sections.values() if 0x20000000<=s['address']<0x20003000)
    frames=[]
    for path in (ROOT/'firmware/build/firmware.elf.ltrans0.ltrans.su',ROOT/'firmware/build/syscalls.su',ROOT/'firmware/build/sysmem.su'):
        for line in path.read_text().splitlines():
            columns=line.split('\t')
            if len(columns)==3:frames.append({'function':columns[0],'bytes':int(columns[1]),'kind':columns[2]})
    memory={'sections':sections,'flash_code_data':flash,'flash_executable_region':28672,'flash_audit_reserve':4096,
            'ram_static_data_bss':sections['.data']['bytes']+sections['.bss']['bytes'],
            'ram_linked_reserve':sections['._user_heap_stack']['bytes'],'ram_linked_total':ram,'ram_total':12288,
            'stack_reserve':4096,'heap_min_reserve':512,'largest_compiler_frames':sorted(frames,key=lambda s:s['bytes'],reverse=True)[:20],
            'runtime_stack':None,'runtime_heap_peak':None,
            'limits':'Compiled frame sizes are not a full call-chain bound; newlib has no .su here. Runtime stack and heap require MCU execution.'}
    (ROOT/'docs/metrics/memory.json').write_text(json.dumps(memory,indent=2))
    symbols=[]
    for line in run('arm-none-eabi-nm','--size-sort','-S','--radix=d',str(ELF)).splitlines():
        parts=line.split()
        if len(parts)!=4:continue
        address,size,kind,name=parts
        if not address.isdigit() or not size.isdigit():continue
        symbols.append({'address':int(address),'size':int(size),'kind':kind,'name':name})
    modules=collections.Counter()
    for symbol in symbols:
        if symbol['kind'].lower()!='t':continue
        info=run('arm-none-eabi-addr2line','-e',str(ELF),hex(symbol['address'])).strip()
        symbol['source']=info
        path=info.rsplit(':',1)[0]
        modules[path]+=symbol['size']
    result={
        'method':'Final linked symbols attributed with DWARF addr2line. LTO may inline other modules into callers; counts describe linked attribution, not original object size.',
        'text_by_linked_source':dict(modules),
        'largest_ram_symbols':sorted([s for s in symbols if s['kind'] in 'bBdD'],key=lambda s:s['size'],reverse=True)[:20],
        'largest_text_symbols':sorted([s for s in symbols if s['kind'] in 'tT'],key=lambda s:s['size'],reverse=True)[:20],
        'unattributed':'rodata, alignment, vector and linker reserves are reported separately by size -A and map.'}
    destination=ROOT/'docs/metrics/symbols.json'
    destination.write_text(json.dumps(result,indent=2))
    print(destination)
