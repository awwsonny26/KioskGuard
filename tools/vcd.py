#!/usr/bin/env python3
"""Decode transitions captured by Wokwi. No synthetic waveforms are generated."""
import argparse, bisect, json, hashlib
from pathlib import Path

def read_vcd(path):
    if "$timescale 1ns $end" not in Path(path).read_text():
        raise ValueError("Expected Wokwi 1ns VCD timescale")
    names={}; traces={}; now=0
    for line in Path(path).read_text().splitlines():
        if line.startswith("$var"):
            p=line.split(); names[p[3]]=p[4]; traces[p[4]]=[]
        elif line.startswith("#"): now=int(line[1:])
        elif line and line[0] in "01" and line[1:] in names:
            tr=traces[names[line[1:]]]
            v=int(line[0])
            if not tr or tr[-1][1]!=v: tr.append((now,v))
    return traces

def level(trace,t):
    i=bisect.bisect_right(trace,(t,2))-1
    return trace[i][1] if i>=0 else 0

def uart(trace,baud=115200):
    period=1e9/baud; result=[]; busy=-1
    for t,v in trace:
        if v or t<busy:continue
        if level(trace,int(t+period/2))!=0:continue
        value=sum(level(trace,int(t+(i+1.5)*period))<<i for i in range(8))
        if level(trace,int(t+9.5*period))!=1:continue
        result.append((t,value));busy=t+9.8*period
    return result

def i2c(scl,sda):
    edges={}
    for t,v in scl:edges.setdefault(t,{})['c']=v
    for t,v in sda:edges.setdefault(t,{})['d']=v
    c=d=1;record=None;bits=[];result=[]
    for t,change in sorted(edges.items()):
        pc,pd=c,d;c=change.get('c',c);d=change.get('d',d)
        if pc==c==1 and pd!=d:
            if d==0:
                if record:
                    record['end']=t;record['repeated']=True;result.append(record)
                record={'start':t,'bytes':[],'acks':[]};bits=[]
            elif record:
                record['end']=t;record['repeated']=False;result.append(record);record=None
        elif record and pc==0 and c==1:
            bits.append(d)
            if len(bits)==9:
                record['bytes'].append(sum(v<<(7-i) for i,v in enumerate(bits[:8])))
                record['acks'].append(bits[8]==0);bits=[]
    return result

def crop(source,dest,start,end):
    lines=Path(source).read_text().splitlines()
    header_end=lines.index('$enddefinitions $end')
    header=lines[:header_end]
    sha=hashlib.sha256(Path(source).read_bytes()).hexdigest()
    header+=['$comment original_sha256='+sha+' offset_ns='+str(start)+' $end',
             '$enddefinitions $end','#0']
    state={};events=[];now=0
    for l in lines[header_end+1:]:
        if l.startswith('#'):now=int(l[1:])
        elif l and l[0] in '01':
            if now<=start:state[l[1:]]=l[0]
            elif now<=end:events.append((now-start,l))
    header += [v+k for k,v in state.items()]
    for t,l in events:header+=['#'+str(t),l]
    header+=['#'+str(end-start)]
    Path(dest).write_text('\n'.join(header)+'\n')

def analyze(source):
    traces=read_vcd(source)
    rx=uart(traces.get('D3',[]));tx=uart(traces.get('D2',[]))
    records=i2c(traces.get('D0',[]),traces.get('D1',[]))
    def lines(data):
        out=[];buffer=[];start=0
        for t,b in data:
            if not buffer:start=t
            if b in (10,13):
                if buffer:
                    out.append({'ns':start,'text':bytes(buffer).decode('ascii',errors='replace').strip()})
                buffer=[]
            else:buffer.append(b)
        return out
    highs=[t for t,v in traces.get('D5',[]) if v]
    return {'i2c':records,'uart_tx':lines(tx),'uart_rx':lines(rx),'adc_rises':highs,
            'channels':{k:len(v) for k,v in traces.items()}}

if __name__=='__main__':
    p=argparse.ArgumentParser()
    p.add_argument('source');p.add_argument('--output',required=True)
    p.add_argument('--start',type=float);p.add_argument('--end',type=float)
    a=p.parse_args()
    if a.start is not None:
        crop(a.source,a.output,int(a.start*1e9),int(a.end*1e9))
    else:
        r=analyze(a.source);Path(a.output+'.json').write_text(json.dumps(r,indent=2))
        Path(a.output+'-uart.txt').write_text('\n'.join(f"{x['ns']/1e6:.6f} ms {x['text']}" for x in r['uart_tx'])+'\n')
        print('Decoded',len(r['i2c']),'I2C transactions and',len(r['uart_tx']),'UART lines')
