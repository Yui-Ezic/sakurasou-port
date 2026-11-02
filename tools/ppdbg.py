#!/usr/bin/env python3
"""Minimal PPSSPP websocket debugger client (stdlib only).
usage: ppdbg.py PORT cmd...   cmds: shot FILE | press BTN [frames] | wait MS | status | resume | read ADDR N"""
import socket,base64,os,struct,json,sys,time
class Dbg:
    def __init__(s,port):
        s.c=socket.create_connection(('127.0.0.1',port),timeout=20)
        k=base64.b64encode(os.urandom(16)).decode()
        s.c.sendall(f'GET /debugger HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: {k}\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Protocol: debugger.ppsspp.org\r\n\r\n'.encode())
        b=b''
        while b'\r\n\r\n' not in b: b+=s.c.recv(4096)
        if b' 101 ' not in b.split(b'\r\n')[0]: raise SystemExit(b)
        s.buf=b[b.index(b'\r\n\r\n')+4:]; s.n=0
    def tx(s,obj):
        d=json.dumps(obj).encode(); m=os.urandom(4)
        h=bytes([0x81]); L=len(d)
        h+=bytes([0x80|L]) if L<126 else bytes([0x80|126])+struct.pack('>H',L) if L<65536 else bytes([0x80|127])+struct.pack('>Q',L)
        s.c.sendall(h+m+bytes(b^m[i&3] for i,b in enumerate(d)))
    def rd(s,n):
        while len(s.buf)<n:
            x=s.c.recv(1<<20)
            if not x: raise EOFError
            s.buf+=x
        r=s.buf[:n]; s.buf=s.buf[n:]; return r
    def rx(s):
        p=b''
        while True:
            a,b=s.rd(2); L=b&127
            if L==126: L=struct.unpack('>H',s.rd(2))[0]
            elif L==127: L=struct.unpack('>Q',s.rd(8))[0]
            d=s.rd(L); op=a&15
            if op==9: continue
            p+=d
            if a&0x80: return json.loads(p)
    def call(s,ev,**kw):
        s.n+=1; t=f't{s.n}'; s.tx(dict(event=ev,ticket=t,**kw))
        while True:
            r=s.rx()
            if r.get('ticket')==t: return r
            if r.get('event')=='error': raise RuntimeError(r)
if __name__=='__main__':
    d=Dbg(int(sys.argv[1])); a=sys.argv[2:]
    i=0
    while i<len(a):
        c=a[i]; i+=1
        if c=='status': print(d.call('cpu.status'))
        elif c=='resume': print(d.call('cpu.resume').get('event'))
        elif c=='stepping': print(d.call('cpu.stepping').get('event'))
        elif c=='wait': time.sleep(int(a[i])/1000); i+=1
        elif c=='press':
            d.call('input.buttons.press',button=a[i],duration=int(a[i+1])); i+=2
        elif c=='shot':
            r=d.call('cpu.stepping'); r=d.call('gpu.buffer.renderColor',type='base64')
            open(a[i],'wb').write(base64.b64decode(r['base64'].split(',',1)[-1])); i+=1
            print({k:v for k,v in r.items() if k!='base64'}); d.call('cpu.resume')
        elif c=='read':
            r=d.call('memory.read',address=int(a[i],0),size=int(a[i+1],0)); print(base64.b64decode(r['base64']).hex(' ')); i+=2
        elif c=='info': print(d.call('game.status'))
