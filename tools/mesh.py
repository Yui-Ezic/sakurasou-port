#!/usr/bin/env python3
"""dump (u,v,x,y,z) meshes from EBOOT: mesh.py addr nverts [texw texh]"""
import struct,sys
d=open(__import__('os').path.join(__import__('os').path.dirname(__import__('os').path.dirname(__import__('os').path.abspath(__file__))),'extracted','EBOOT.BIN.dec'),'rb').read()
a=int(sys.argv[1],16);n=int(sys.argv[2]);tw=int(sys.argv[3]) if len(sys.argv)>3 else 1;th=int(sys.argv[4]) if len(sys.argv)>4 else 1
for i in range(n):
    u,v,x,y,z=struct.unpack('<5f',d[0xc0+a+i*20:0xc0+a+i*20+20])
    print(f'{i}: uv px ({u*tw:.1f},{v*th:.1f}) xy ({x:.2f},{y:.2f})')
