import ctypes, sys
# the real text renderer (textscr.c): red background, white heading and text
L = ctypes.CDLL('./libslides.so')
img = (ctypes.c_uint8*10000)()
lines = (ctypes.c_char*17*7)()
txt = ["Battery 3.012 V","FW 69.9/80 KB","Slides 6","Memory 44/392 KB","Flash 512 KB","RAM 19659 B free","TX power +3 dBm"]
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
check(cnt[2]==0 and cnt[0]==0,'text screen: only red and white')
check(cnt[1]>1500,'text screen: white text (%d px)'%cnt[1])
check(cnt[3]>25000,'text screen: red background (%d px)'%cnt[3])
# the last glyph fits: the right margin stays red; the heading and the rule are white
check(all(px(x,y)==3 for y in range(200) for x in range(197,200)),'text screen: right margin red')
check(any(px(x,y)==1 for y in range(0,30) for x in range(200)),'text screen: white heading')
if '-v' in sys.argv:
    for y in range(0,200,2):
        print(''.join(' .#R'[px(x,y)] if True else '' for x in range(0,200,2)))
sys.exit(0 if ok else 1)
