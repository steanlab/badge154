import json,sys
sys.argv=['x']
src=open('t.py').read().split("random.seed(1)")[0]
exec(src)
host_reset=L.host_reset; host_reset(); L.slides_init(); L.slides_tick()
d=json.load(open('codec_out.json')); slot=0; allok=True
for k,v in d.items():
    data=bytes.fromhex(v['data']); exp=bytes.fromhex(v['expect'])
    assert crc16(data)==v['crc']
    r=upload(slot,data,v['flags'],v['pal0'],v['pal1'])
    cmd(0x25,slot); L.slides_tick()
    good = r[1]==0 and bytes(shown)==exp
    allok&=good; print('PASS' if good else 'FAIL', k, 'res',r[1], 'bytes',len(data)); slot+=1
info=cmd(0x28); print('slides',info[3],'free sectors',info[5],'violations',ctypes.c_int.in_dll(L,'write_violations').value)
print('JS->C ALL OK' if allok else 'FAILURES')
sys.exit(0 if allok else 1)
