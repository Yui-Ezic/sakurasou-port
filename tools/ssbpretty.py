#!/usr/bin/env python3
"""Compact expression-level listing of an SSB: folds pushes into call args."""
import sys,os
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from ssbdis import *
d,w=load(sys.argv[1]); c=walk(w)
st=[]
BIN={48:'+',49:'-',50:'*',51:'/',52:'%',53:'|',54:'&',55:'^',57:'<<',58:'>>',61:'||',62:'&&',64:'==',65:'!=',66:'>',67:'<',68:'>=',69:'<='}
tg=set()
for pc,(op,a) in c.items():
    if op in(8,9): tg.add(a[0])
    if op in(10,11): tg.add(a[1])
    if op==13: tg.add(a[1])
prev=None
for pc in sorted(c):
    op,a=c[pc]
    if pc in tg:
        if st: print(f'  ;stack {st}'); st=[]
        print(f'L{pc:x}:')
    if op==2: st.append(str(a[0] if a[0]<0x80000000 else a[0]-(1<<32)))
    elif op==3: st.append('"'+s(d,a[0]).replace('\n','\\n')+'"')
    elif op==4: st.append(f'l{a[0]}')
    elif op==5: st.append(f'g{a[0]}')
    elif op==14:
        n=a[0]; ar=st[len(st)-n:] if n else []; st=st[:len(st)-n] if n else st
        st.append(f'N{a[1]}({", ".join(ar)})')
    elif op==13:
        n=a[0]; ar=st[len(st)-n:] if n else []; st=st[:len(st)-n] if n else st
        st.append(f'F{a[1]:x}({", ".join(ar)})')
    elif op==6:
        print('  '+(st.pop() if st else '<pop>'))
    elif op in BIN and len(st)>=2:
        b=st.pop();x=st.pop();st.append(f'({x} {BIN[op]} {b})')
    elif op==60 and st: st.append('-'+st.pop())
    elif op==63 and st: st.append('!'+st.pop())
    elif op==56 and st: st.append('~'+st.pop())
    elif op in(18,19):
        v=st.pop() if st else '?'
        st.append(f'{"l" if op==18 else "g"}{a[0]}={v}')
    elif 20<=op<=47:
        v=st.pop() if st and op<40 else ''
        st.append(f'op{op}:{"lg"[op&1]}{a[0]}<{v}>')
    elif op==9: print(f'  jz {st.pop() if st else "?"} -> L{a[0]:x}')
    elif op==8: print(f'  jmp L{a[0]:x}')
    elif op in(10,11): print(f'  unwind{op} {a[0]} -> L{a[1]:x}')
    elif op==15: print(f'  ret {st.pop() if st else ""}'); st=[]
    elif op==1: print('  yield')
    elif op==0: print('  end')
    elif op==16: print('  mark')
    elif op==17: print('  unmark'); st=[]
    else: print(f'  op{op} {a} stack={st}')
