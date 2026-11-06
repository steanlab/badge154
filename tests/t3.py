import ctypes, sys
# the real text renderer (textscr.c): background black, heading red, text white
L = ctypes.CDLL('./libslides.so')
img = (ctypes.c_uint8*10000)()
lines = (ctypes.c_char*17*7)()
txt = ["Battery 3.012 V","Firmware 69.9 KB","Slides 6","Blocks 11/98","Flash 512 KB","RAM 19659 B free","TX power +3 dBm"]
for i,s in enumerate(txt): lines[i].value = s.encode()
L.textscr_draw.argtypes=[ctypes.c_void_p, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_uint8]
L.textscr_draw(img, b"TAG HARDWARE", ctypes.byref(lines), 7)
def px(x,y): return (img[y*50+x//4] >> (6-2*(x&3))) & 3
cnt={0:0,1:0,2:0,3:0}
for y in range(200):
    for x in range(200): cnt[px(x,y)]+=1
ok = True
def check(c,m):
    global ok
    print(('PASS ' if c else 'FAIL ')+m)
    if not c: ok=False
check(cnt[2]==0,'text screen: no yellow')
check(cnt[3]>300,'text screen: red heading and rule (%d px)'%cnt[3])
check(cnt[1]>1500,'text screen: white text (%d px)'%cnt[1])
check(cnt[0]>30000,'text screen: black background (%d px)'%cnt[0])
# red only in the heading area, white only below it
check(all(px(x,y)!=3 for y in range(30,200) for x in range(200)),'text screen: red only in the heading')
check(all(px(x,y)!=1 for y in range(0,30) for x in range(200)),'text screen: no white in the heading')
# nothing is drawn outside the 200x200 image and the last glyph fits: right margin stays black
check(all(px(x,y)==0 for y in range(200) for x in range(197,200)),'text screen: right margin black')
if '-v' in sys.argv or True:
    for y in range(0,200,2):
        print(''.join(' .#R'[px(x,y)] if True else '' for x in range(0,200,2)))
sys.exit(0 if ok else 1)