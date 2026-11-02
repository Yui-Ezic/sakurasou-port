#!/usr/bin/env python3
import sys,glob,re,os
from PIL import Image
d=sys.argv[1]; out=sys.argv[2]; cols=int(sys.argv[3]) if len(sys.argv)>3 else 3
fs=sorted(glob.glob(d+'/*.bmp'),key=lambda f:int(re.findall(r'(\d+)\.bmp',f)[0]))
if len(sys.argv)>4:
    n=int(sys.argv[4]); st=max(1,len(fs)//n); fs=fs[::st][:n]
ims=[Image.open(f).convert('RGB') for f in fs]
rows=(len(ims)+cols-1)//cols
M=Image.new('RGB',(cols*482,rows*274),(255,0,255))
for i,im in enumerate(ims): M.paste(im,((i%cols)*482,(i//cols)*274))
M.save(out)
