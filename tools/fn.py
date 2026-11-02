#!/usr/bin/env python3
"""Print decompiled funcs: fn.py addr[-addr] ...   Uses work/eboot_decomp.c"""
import re,sys,os,pickle
base=os.path.join(os.path.dirname(os.path.abspath(__file__)),'..','work')
s=open(base+'/eboot_decomp.c',errors='replace').read()
fs={}
for f in re.split(r'(?m)^//@@ ',s)[1:]:
    fs[int(f[:8],16)]=re.sub(r'\n\s*\n','\n',f)
k=sorted(fs)
for a in sys.argv[1:]:
    if '-' in a:
        x,y=[int(t,16) for t in a.split('-')]
        for q in k:
            if x<=q<y: print(fs[q])
    else:
        x=int(a,16)
        import bisect
        print(fs[k[bisect.bisect_right(k,x)-1]])
