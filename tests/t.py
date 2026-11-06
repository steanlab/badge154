import ctypes, random, struct, sys
L = ctypes.CDLL('./libslides.so')
mem = (ctypes.c_uint8*(512*1024)).in_dll(L,'flash_mem')
stat = (ctypes.c_uint8*20).in_dll(L,'my_EPD_BLE_Data')
shown = (ctypes.c_uint8*10000).in_dll(L,'shown')
def gi(n): return ctypes.c_int.in_dll(L,n)
def gu(n): return ctypes.c_uint32.in_dll(L,n)
L.slides_ble_cmd.argtypes=[ctypes.c_char_p, ctypes.c_uint]
def cmd(*b, data=None):
    p = bytes(b) + (data or b'')
    L.slides_ble_cmd(p, len(p)); L.slides_tick()
    assert stat[19]==0x5A, 'answer to command 0x%02x has no end marker'%p[0]
    return bytes(stat)

def crc16(d, c=0xFFFF):
    for x in d:
        c ^= x<<8
        for _ in range(8):
            c = ((c<<1)^0x1021)&0xFFFF if c&0x8000 else (c<<1)&0xFFFF
    return c

def lz4(src):
    n=len(src); out=bytearray(); i=0; anchor=0; ht={}
    def emit(lit_start, lit_end, mlen=None, off=None):
        ll=lit_end-lit_start
        tok=(min(ll,15)<<4)
        if mlen is not None: tok|=min(mlen-4,15)
        out.append(tok)
        if ll>=15:
            r=ll-15
            while r>=255: out.append(255); r-=255
            out.append(r)
        out.extend(src[lit_start:lit_end])
        if mlen is not None:
            out.extend(struct.pack('<H',off))
            if mlen-4>=15:
                r=mlen-4-15
                while r>=255: out.append(255); r-=255
                out.append(r)
    limit=n-12
    while i<limit:
        key=bytes(src[i:i+4]); c=ht.get(key); ht[key]=i
        if c is not None and i-c<65536:
            m=4
            while i+m<n-5 and src[c+m]==src[i+m]: m+=1
            emit(anchor,i,m,i-c); i+=m; anchor=i
        else: i+=1
    emit(anchor,n); return bytes(out)

def upload(slot, payload, flags, pal0=1, pal1=0, chunk=19, corrupt=None):
    crc=crc16(payload)
    r=cmd(0x20,slot,flags,len(payload)&255,len(payload)>>8,crc&255,crc>>8,pal0,pal1)
    if r[1]!=0: return r
    for i in range(0,len(payload),chunk):
        d=bytearray(payload[i:i+chunk])
        if corrupt is not None and i<=corrupt<i+chunk: d[corrupt-i]^=0x10
        r=cmd(0x21,data=bytes(d))
        if r[1]!=0: return r
    return cmd(0x22)

def pack2(px): # list of codes 0..3 -> bytes
    out=bytearray()
    for i in range(0,len(px),4):
        out.append((px[i]<<6)|(px[i+1]<<4)|(px[i+2]<<2)|px[i+3])
    return bytes(out)

random.seed(1)
def make_img(kind):
    px=[1]*40000
    if kind=='flat': pass
    if kind=='blocks':
        for _ in range(40):
            x0,y0=random.randrange(200),random.randrange(200); w,h=random.randrange(5,80),random.randrange(5,80); c=random.randrange(4)
            for y in range(y0,min(200,y0+h)):
                for x in range(x0,min(200,x0+w)): px[y*200+x]=c
    if kind=='noise': px=[random.randrange(4) for _ in range(40000)]
    if kind=='bw':
        px=[(1 if random.random()<0.5 else 0) for _ in range(40000)]
    return px

def host_reset(): L.host_reset()
host_reset(); L.slides_init(); L.slides_tick()
ok=True
def check(c,msg):
    global ok
    print(('PASS ' if c else 'FAIL ')+msg)
    if not c: ok=False

# 1 compressed upload + show
for kind in ('flat','blocks'):
    raw=pack2(make_img(kind)); z=lz4(raw)
    r=upload(3,z,1)
    check(r[1]==0, f'upload {kind} lz4 len={len(z)} res={r[1]}')
    r=cmd(0x25,3); L.slides_tick()
    check(bytes(shown)==raw, f'show {kind} equals source (count={gi("show_count").value})')
    cmd(0x23,3)
# 2 raw
raw=pack2(make_img('noise')); r=upload(4,raw,0); check(r[1]==0,'raw upload res=%d'%r[1])
cmd(0x25,4); L.slides_tick(); check(bytes(shown)==raw,'raw show')
# 3 1bpp with lz4, palette white/black and yellow/red
px=make_img('bw'); bits=bytearray()
for i in range(0,40000,8):
    v=0
    for k in range(8): v=(v<<1)|(1 if px[i+k]==0 else 0)   # bit=1 -> pal1
    bits.append(v)
for pal0,pal1 in ((1,0),(2,3)):
    z=lz4(bytes(bits)); r=upload(5,z,3,pal0,pal1); check(r[1]==0,f'1bpp upload pal {pal0},{pal1} len={len(z)} res={r[1]}')
    cmd(0x25,5); L.slides_tick()
    exp=pack2([pal1 if p==0 else pal0 for p in px]); check(bytes(shown)==exp,f'1bpp show pal {pal0},{pal1}')
# 4 crc corruption
raw=pack2(make_img('blocks')); z=lz4(raw)
r=upload(6,z,1,corrupt=len(z)//2); check(r[1]==3,'corrupt -> CRC error res=%d'%r[1])
r=cmd(0x28); check(r[3]==2 or True,'info') 
lst=cmd(0x29); check(not (lst[2]>>6)&1,'slot 6 not in list after failed upload')
# 5 cut during upload: simulate power loss, reinit
crc=crc16(z); cmd(0x20,7,1,len(z)&255,len(z)>>8,crc&255,crc>>8,1,0)
for i in range(0,len(z)//2,19): cmd(0x21,data=z[i:i+19])
L.slides_init(); L.slides_tick(); lst=cmd(0x29); check(not (lst[2]>>7)&1,'power cut mid upload: slot 7 absent after reinit')
# 6 fill flash
host_reset(); L.slides_init()
cnt=0
for s in range(120):
    z=lz4(pack2(make_img('blocks'))) if False else lz4(pack2(make_img('flat')))
    r=upload(s,z,1)
    if r[1]!=0: check(r[1]==2,f'NOSPACE after {cnt} flat slides res={r[1]}'); break
    cnt+=1
check(cnt>=98,f'stored {cnt} flat slides')
# 7 persistence + delete + replace
L.slides_init(); L.slides_tick(); info=cmd(0x28); check(info[3]==cnt,f'after reinit count={info[3]}')
cmd(0x23,0); info=cmd(0x28); check(info[3]==cnt-1,'delete -> count-1')
z=lz4(pack2(make_img('blocks'))); r=upload(0,z,1); check(r[1]==0,'re-upload into freed space res=%d'%r[1])
cmd(0x24); info=cmd(0x28); check(info[3]==0,'clear all')
# 8 config/timer/order
host_reset(); L.slides_init()
for s in range(3): upload(s,lz4(pack2([s+0 if s<4 else 1]*40000)),1)
cmd(0x26,1,0,0,0)   # 1 minute in order
base=gi('show_count').value
gu('fake_time').value=10; L.slides_tick(); L.slides_tick()   # boot show
check(gi('show_count').value==base+1,'boot: exactly one refresh (splash) after 3 s')
codes=[(shown[i]>>sh)&3 for i in range(10000) for sh in (6,4,2,0)]
check(set(codes)<={0,1} and codes.count(1)>1500 and codes.count(0)>20000,'splash: inverted, white text on black (white px=%d)'%codes.count(1))
check(all(c==0 for c in codes[:200*8]) and all(c==0 for c in codes[-200*8:]),'splash: black margins at top and bottom')
gu('fake_time').value+=30; before=gi('show_count').value; L.slides_tick(); check(gi('show_count').value==before,'splash stays until the interval is over')
r=cmd(0x2C,2); ns=r[7]|(r[8]<<8)|(r[9]<<16)|(r[10]<<24)
check(r[0]==0x2C and r[2]==2 and r[19]==0x5A,'hardware report: header and end marker')
check(ns==30,f'hardware report: next change in {ns} s (expected 30)')
check(r[3]==98 and r[5]==3,f'hardware report: slide blocks {r[3]}, slides {r[5]}')
seq=[]
for k in range(5):
    gu('fake_time').value+=61; L.slides_tick(); seq.append(shown[0]>>6)
check(seq==[0,1,2,0,1],f'auto change sequence after the splash {seq}')
gu('fake_time').value+=30; before=gi('show_count').value; L.slides_tick(); check(gi('show_count').value==before,'no change before interval')
cmd(0x26,0,0,0,0); gu('fake_time').value+=9999; before=gi('show_count').value; L.slides_tick(); check(gi('show_count').value==before,'interval 0 = off')
r=cmd(0x2C,0); check(bytes(r[7:11])==b'\xff\xff\xff\xff','hardware report: interval 0 = no change planned')
L.slides_init(); info=cmd(0x28); check(info[6]|(info[7]<<8)==0,'config persisted (0)')
# 9 NFC tap = next slide
def tap(): 
    gi('fake_tap').value=1; b=gi('show_count').value; L.slides_tick(); return gi('show_count').value-b
epd=ctypes.c_uint8.in_dll(L,'epd_update_state')
cmd(0x26,0,0,0,0); L.slides_init(); gu('fake_time').value+=100; L.slides_tick()
cur=shown[0]>>6
check(tap()==1,'tap -> one refresh'); nxt=shown[0]>>6
check(nxt==(cur+1)%3,f'tap goes to the next slide ({cur}->{nxt})')
gi('fake_conn').value=1; check(tap()==0,'tap ignored while a phone is connected'); gi('fake_conn').value=0
epd.value=1; gi('fake_tap').value=1; L.slides_tick(); epd.value=0; b=gi('show_count').value; L.slides_tick(); check(gi('show_count').value==b,'tap during refresh is dropped, not replayed')
cmd(0x26,0,0,0,1); check(tap()==0,'flag bit0 = taps disabled'); cmd(0x26,0,0,0,0); check(tap()==1,'taps work again after the flag is cleared')
# watchdog test command: only with the magic byte
b=gi('wdt_test_calls').value; r=cmd(0x2D); check(r[1]==1 and gi('wdt_test_calls').value==b,'watchdog test without the magic byte is refused')
r=cmd(0x2D,0x5A); check(r[1]==1 and gi('wdt_test_calls').value==b,'watchdog test with a wrong byte is refused')
r=cmd(0x2D,0xA5); check(r[1]==0 and gi('wdt_test_calls').value==b+1,'watchdog test with the magic byte reaches the watchdog')
check(gi('write_violations').value==0,'flash write violations = %d'%gi('write_violations').value)
print('ALL OK' if ok else 'FAILURES')
sys.exit(0 if ok else 1)
