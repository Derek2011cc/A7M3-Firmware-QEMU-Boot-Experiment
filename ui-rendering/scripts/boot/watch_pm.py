import os, socket, time, json, sys

qmp = os.environ.get('PM_QMP', '/tmp/a7m3-qmp7.qmp')
log = os.environ.get('PM_LOG', '/mnt/f/a7c2-re-main/a7m3-qemu-logs/a7m3-uiprobe8.log')
dumps = [
    (0x202e0200, 0x96000, '/tmp/a7m3-pm-disp.raw'),
    (0x226e0000, 0x100000, '/tmp/a7m3-pm-tiles.raw'),
    (0x22770000, 0x20000, '/tmp/a7m3-pm-mini.raw'),
    (0x228f0000, 0x10000, '/tmp/a7m3-pm-tex228f.raw'),
    (0x222e0200, 0x96000, '/tmp/a7m3-pm-bg.raw'),
    (0x2055f000, 0x40000, '/tmp/a7m3-pm-gpu1.raw'),
    (0x20530000, 0x40000, '/tmp/a7m3-pm-gpu2.raw'),
]
deadline = time.time() + 900
seen = False
while time.time() < deadline:
    try:
        d = open(log, 'rb').read()
    except FileNotFoundError:
        d = b''
    if b'SCN done' in d:
        seen = True
        break
    time.sleep(1)
print('scn_done', seen, flush=True)
if not seen:
    sys.exit(1)
time.sleep(40)
s = None
for _ in range(240):
    try:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.connect(qmp)
        break
    except OSError:
        s = None
        time.sleep(0.5)
if s is None:
    print('qmp connect failed', flush=True)
    sys.exit(1)
s.settimeout(20)
buf = b''
def rdline():
    global buf
    while b'\n' not in buf:
        d = s.recv(65536)
        if not d:
            return b''
        buf += d
    line, buf = buf.split(b'\n', 1)
    return line.strip()
print('greet', rdline(), flush=True)
s.sendall(b'{"execute": "qmp_capabilities"}\n')
print('cap', rdline(), flush=True)
for val, size, fn in dumps:
    s.sendall(json.dumps({'execute': 'pmemsave',
                          'arguments': {'val': val, 'size': size,
                                        'filename': fn}}).encode() + b'\n')
    r = rdline()
    print('dump %08x %x -> %s : %s' % (val, size, fn, r), flush=True)
s.close()
for val, size, fn in dumps:
    print('saved', fn, os.path.getsize(fn) if os.path.exists(fn) else -1, flush=True)
