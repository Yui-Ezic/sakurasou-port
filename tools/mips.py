#!/usr/bin/env python3
"""tiny allegrex disasm for reading short handlers: mips.py addr count"""
import struct,sys
d=open('/home/misha/dev/Sakurasou/extracted/EBOOT.BIN.dec','rb').read()
R='zero at v0 v1 a0 a1 a2 a3 t0 t1 t2 t3 t4 t5 t6 t7 s0 s1 s2 s3 s4 s5 s6 s7 t8 t9 k0 k1 gp sp fp ra'.split()
def dis(a,w):
    op=w>>26;rs=(w>>21)&31;rt=(w>>16)&31;rd=(w>>11)&31;sh=(w>>6)&31;fn=w&63;imm=w&0xffff;si=imm-65536 if imm&0x8000 else imm
    if op==0:
        n={0:'sll',2:'srl',3:'sra',8:'jr',9:'jalr',0x21:'addu',0x23:'subu',0x24:'and',0x25:'or',0x26:'xor',0x2a:'slt',0x2b:'sltu',0x1a:'div',0x18:'mult',0x12:'mflo',0x10:'mfhi'}.get(fn,f'fn{fn:x}')
        return f'{n} {R[rd]},{R[rs]},{R[rt]} sh={sh}'
    if op==3: return f'jal {((w&0x3ffffff)<<2):x}'
    if op==2: return f'j {((w&0x3ffffff)<<2):x}'
    n={8:'addi',9:'addiu',10:'slti',11:'sltiu',12:'andi',13:'ori',15:'lui',35:'lw',43:'sw',32:'lb',36:'lbu',33:'lh',37:'lhu',40:'sb',41:'sh',4:'beq',5:'bne'}.get(op,f'op{op:x}')
    if op in(35,43,32,36,33,37,40,41): return f'{n} {R[rt]},{si}({R[rs]})'
    if op in(4,5): return f'{n} {R[rs]},{R[rt]},{a+4+si*4:x}'
    return f'{n} {R[rt]},{R[rs]},{si}'
a=int(sys.argv[1],16)
for i in range(int(sys.argv[2])):
    w=struct.unpack('<I',d[0xc0+a+i*4:0xc0+a+i*4+4])[0]
    print(f'{a+i*4:x} {w:08x} {dis(a+i*4,w)}')
