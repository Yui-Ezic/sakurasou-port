import sys,glob,collections,re,os
sys.path.insert(0,os.path.dirname(__file__))
from ssbdis import *
st=collections.Counter(); ar=collections.defaultdict(collections.Counter); ex={}
bad=0
for f in sorted(glob.glob('data/*.ssb')):
    d,w=load(f); c=walk(w)
    keys=sorted(c)
    for i,pc in enumerate(keys):
        op,a=c[pc]
        if op=='BAD': bad+=1; continue
        if op==14:
            st[a[1]]+=1; ar[a[1]][a[0]]+=1
            if a[1] not in ex or len(ex[a[1]])<4:
                pa=[]
                j=i-1;n=a[0]
                while j>=0 and n>0:
                    o,b=c[keys[j]]
                    if o==2: pa.append(str(b[0]))
                    elif o==3: pa.append(repr(s(d,b[0])[:24]))
                    else: pa.append('?')
                    j-=1;n-=1
                ex.setdefault(a[1],[]).append(','.join(reversed(pa)))
print('bad',bad)
src=open('eboot_decomp.c',errors='replace').read()
m=re.findall(r'case (0x[0-9a-f]+|\d+):\s*\n\s*uVar1 = FUN_([0-9a-f]+)',open('a318.txt').read())
tab={int(k,0):v for k,v in m}
for k in sorted(set(st)|set(tab)):
    print(k,hex(k),tab.get(k),st[k],dict(ar[k]),ex.get(k,[])[:3])
