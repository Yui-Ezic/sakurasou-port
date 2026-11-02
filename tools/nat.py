import struct,re,sys
d=open('../extracted/EBOOT.BIN.dec','rb').read()
OFF=0xc0
V={'base':0x22cbc4,'prog':0x22d99c,'act':0x22e30c,'stat':0x22f484,'adv':0x2306f4}
def slot(v,n): return struct.unpack('<I',d[OFF+v+0x14+8*n:OFF+v+0x18+8*n])[0]
src=open('eboot_decomp.c',errors='replace').read()
fs={}
for f in re.split(r'(?m)^//@@ ',src)[1:]:
    fs[int(f[:8],16)]=f
def body(a):
    f=fs.get(a)
    if f is None: return f'<{a:x} nodef>'
    b=f[f.index('{')+1:]
    b=re.sub(r'\n\s*(undefined\d?|int|uint|bool|float|char|short|byte|ushort)\s+\*?\*?\w+(\s*\[\w+\])?;','',b)
    return re.sub(r'\s+',' ',b)
lo,hi=int(sys.argv[1]),int(sys.argv[2])
for n in range(lo,hi):
    for k in sys.argv[3:]:
        a=slot(V[k],n); print(f'#{n} {k} {a:x}: {body(a)}')
