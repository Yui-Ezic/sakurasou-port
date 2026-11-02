import struct,re,sys
d=open('../extracted/EBOOT.BIN.dec','rb').read()
OFF=0xc0
src=open('eboot_decomp.c',errors='replace').read()
fa=set(int(x,16) for x in re.findall(r'(?m)^//@@ ([0-9a-f]{8})',src))
seg0=2343532
words=struct.unpack('<%dI'%(seg0//4),d[OFF:OFF+seg0//4*4])
res=[]
i=0
n=len(words)
while i<n-4:
    j=i
    while j+1<n and words[j+1] in fa and (words[j]&0xffff)==0:
        j+=2
    c=(j-i)//2
    if c>=100: res.append((i*4,c)); i=j
    else: i+=1
print([(hex(a),c) for a,c in res])
import pickle
pickle.dump(res,open('vt.pkl','wb'))
res=[]
i=0
while i<n-4:
    j=i
    while j+3<n and words[j+1] in fa and words[j+3] in fa:
        j+=2
    c=(j-i)//2+1
    if c>=60: res.append((i*4,c)); i=j+2
    else: i+=1
print([(hex(a),c,hex(words[a//4])) for a,c in res])
pickle.dump(res,open('vt.pkl','wb'))
