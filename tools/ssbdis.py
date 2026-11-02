#!/usr/bin/env python3
"""SSB script disassembler / native-call statistics."""
import struct,sys,glob,os,collections
LEN={0:1,1:1,2:2,3:2,4:2,5:2,6:1,7:0,8:2,9:2,10:3,11:3,12:0,13:3,14:3,15:1,16:1,17:1}
for i in range(18,48): LEN[i]=2
for i in range(48,70): LEN[i]=1
NAMES={0:'end',1:'yield',2:'pushi',3:'pushs',4:'pushl',5:'pushg',6:'pop',8:'jmp',9:'jz',10:'unwind_j',11:'unwind_jb',13:'call',14:'ncall',15:'ret',16:'pushmark',17:'popmark',18:'stl',19:'stg'}
def load(path):
    d=open(path,'rb').read()
    return d,struct.unpack('<%dI'%(len(d)//4),d[:len(d)//4*4])
def walk(w):
    """recursive traversal from pc=0 following jumps/calls; returns dict pc->(op,args)"""
    seen={}; todo=[0]
    while todo:
        pc=todo.pop()
        while pc<len(w) and pc not in seen:
            op=w[pc]
            if op not in LEN or LEN[op]==0:
                seen[pc]=('BAD',op); break
            a=w[pc+1:pc+LEN[op]]
            seen[pc]=(op,a)
            if op in(8,):
                todo.append(a[0]); break
            if op==9: todo.append(a[0])
            if op in(10,11): todo.append(a[1]); break
            if op==13: todo.append(a[1])
            if op in(0,15): break
            pc+=LEN[op]
    return seen
def s(d,a):
    a*=4
    if a>=len(d): return '?'
    e=d.find(b'\0',a); return d[a:e].decode('utf8','replace')
if __name__=='__main__':
    d,w=load(sys.argv[1]); c=walk(w)
    for pc in sorted(c):
        op,a=c[pc]
        if op=='BAD': print(f'{pc:06x} BAD {a}'); continue
        ex=f' ; "{s(d,a[0])}"' if op==3 else ''
        print(f'{pc:06x} {NAMES.get(op,"op%d"%op)} '+' '.join(hex(x) for x in a)+ex)
