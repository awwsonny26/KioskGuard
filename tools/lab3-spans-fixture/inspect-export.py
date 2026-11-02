#!/usr/bin/env python3
"""Decode actual 64-slot UART export; no missing record is treated as valid."""
import argparse,json,re,struct,zlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('uart',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
text=a.uart.read_text(errors='replace')
records=re.findall(r'^EXPORT slot=(\d+) ok=(\d+) data=([0-9a-f]+)\r?$',text,re.M)
last={int(slot):(int(ok),bytes.fromhex(data)) for slot,ok,data in records}
valid=[];erased=[];invalid=[]
for slot,(ok,record) in sorted(last.items()):
 if not ok or len(record)!=64:invalid.append(slot);continue
 if record==b'\xff'*64:erased.append(slot);continue
 magic,generation=struct.unpack_from('<II',record)
 crc=struct.unpack_from('<I',record,52)[0]
 if magic!=0x3341474b or crc!=zlib.crc32(record[:52]) or record[56:]!=bytes.fromhex('334d4f43ccb2b0bc'):
  invalid.append(slot);continue
 valid.append({'slot':slot,'generation':generation})
result={'slots_seen':len(last),'lines_seen':len(records),'valid':valid,'erased':erased,'invalid':invalid,
        'limitations':'UART export is a snapshot of simulated flash, not physical persistence or power loss.'}
a.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
