#!/usr/bin/env python3
import socket, sys, time, re

logpath = sys.argv[1]
duration = float(sys.argv[2]) if len(sys.argv) > 2 else 240
outpath = sys.argv[3] if len(sys.argv) > 3 else '/tmp/rsp_kickstack3.log'
PORT = int(sys.argv[4]) if len(sys.argv) > 4 else 1234

def csum(s):
    return sum(s.encode()) & 0xff

class RSP:
    def __init__(self):
        self.s = None
        for _ in range(900):
            try:
                self.s = socket.create_connection(('127.0.0.1', PORT), timeout=2)
                break
            except OSError:
                time.sleep(0.5)
        if self.s is None:
            raise SystemExit('connect failed')
        self.s.settimeout(0.5)
        self.buf = b''
    def raw(self, d): self.s.sendall(d)
    def send(self, pkt): self.s.sendall(('$%s#%02x' % (pkt, csum(pkt))).encode())
    def recv_pkt(self, timeout=2):
        end = time.time() + timeout
        while time.time() < end:
            try:
                d = self.s.recv(16384)
            except socket.timeout:
                continue
            if not d:
                return None
            self.buf += d
            if self.buf[:1] in (b'+', b'-'):
                self.buf = self.buf[1:]
            while b'$' in self.buf:
                i = self.buf.index(b'$')
                j = self.buf.find(b'#', i)
                if j < 0 or len(self.buf) < j + 3:
                    break
                pkt = self.buf[i+1:j].decode('latin1')
                self.buf = self.buf[j+3:]
                try: self.s.sendall(b'+')
                except Exception: pass
                return pkt
        return None
    def cmd(self, pkt, timeout=2):
        self.send(pkt); return self.recv_pkt(timeout)
    def interrupt(self):
        self.raw(b'\x03'); return self.recv_pkt(2)
    def mem(self, addr, n, timeout=2):
        r = self.cmd('m%x,%x' % (addr, n), timeout)
        if not r or r.startswith('E'): return b''
        try: return bytes.fromhex(r)
        except Exception: return b''

def rd32(r, a):
    b = r.mem(a, 4)
    return int.from_bytes(b, 'little') if len(b) == 4 else None

log = open(outpath, 'a')
def w(m):
    log.write(m + '\n'); log.flush()

r = RSP(); time.sleep(0.3)
p = r.recv_pkt(1)
if p is None:
    r.send('?'); p = r.recv_pkt(1)
w('initial %s' % p)
r.send('c')

base = None
armed = False
bp = None
start = time.time()
uipat = re.compile(rb'(?:UIINFO|HINFO) ([0-9a-f]{8})')
hits = 0
while time.time() - start < duration:
    try:
        d = open(logpath, 'rb').read()
    except FileNotFoundError:
        d = b''
    if base is None:
        m = uipat.search(d)
        if m:
            base = int(m.group(1), 16)
            w('libObj base %08x' % base)
    if not armed and base:
        st = r.interrupt()
        w('int %s' % st)
        bp = base + 0x73a290
        resp = r.cmd('Z0,%x,2' % bp, 2)
        w('Z0 kick %08x -> %s' % (bp, resp))
        armed = True
        r.send('c')
        continue
    pkt = r.recv_pkt(0.5)
    if not pkt:
        continue
    if pkt.startswith('T'):
        hits += 1
        g = r.cmd('g', 2)
        if not g:
            w('stop %s no regs' % pkt); r.send('c'); continue
        raw = bytes.fromhex(g)
        regs = [int.from_bytes(raw[i*4:(i+1)*4], 'little') for i in range(16)]
        pc, sp, job, lr = regs[15], regs[13], regs[0], regs[14]
        j0 = rd32(r, job) if job else None
        j1 = rd32(r, job + 4) if job else None
        j2 = rd32(r, job + 8) if job else None
        first = rd32(r, j0) if j0 else None
        w('hit%d pc=%08x lr=%08x sp=%08x job=%08x job0=%s job1=%s job2=%s first=%s' % (
            hits, pc, lr, sp, job,
            j0 is not None and '%08x' % j0, j1 is not None and '%08x' % j1,
            j2 is not None and '%08x' % j2, first is not None and '%08x' % first))
        menu = first in (0x0042f000, 0x0041f000)
        if menu or hits <= 2:
            stk = r.mem(sp & ~3, 512)
            w('stack ' + stk.hex())
            vals = []
            for i in range(0, len(stk) - 3, 4):
                v = int.from_bytes(stk[i:i+4], 'little')
                if base <= v < base + 0x1400000:
                    vals.append('sp+%03x=%08x(off %06x)' % (i, v, v - base))
            w('libobj_stack ' + ' '.join(vals))
            w('code ' + r.mem(pc, 24).hex())
        if menu:
            w('MENU KICK FOUND')
            break
        r.cmd('z0,%x,2' % bp, 1)
        r.send('s')
        st = r.recv_pkt(2)
        r.cmd('Z0,%x,2' % bp, 1)
        r.send('c')
    elif pkt.startswith('W'):
        w('exited %s' % pkt); break
w('END armed=%s base=%s hits=%d' % (armed, base and '%08x' % base, hits))
log.close()
