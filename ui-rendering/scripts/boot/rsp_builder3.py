#!/usr/bin/env python3
import socket, sys, time, re

logpath = sys.argv[1]
duration = float(sys.argv[2]) if len(sys.argv) > 2 else 240
outpath = sys.argv[3] if len(sys.argv) > 3 else '/tmp/rsp_builder3.log'
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
    def raw(self, data):
        self.s.sendall(data)
    def send(self, pkt):
        self.s.sendall(('$%s#%02x' % (pkt, csum(pkt))).encode())
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
                try:
                    self.s.sendall(b'+')
                except Exception:
                    pass
                return pkt
        return None
    def cmd(self, pkt, timeout=2):
        self.send(pkt)
        return self.recv_pkt(timeout)
    def interrupt(self):
        self.raw(b'\x03')
        return self.recv_pkt(2)
    def mem(self, addr, n, timeout=2):
        r = self.cmd('m%x,%x' % (addr, n), timeout)
        if not r or r.startswith('E'):
            return b''
        try:
            return bytes.fromhex(r)
        except Exception:
            return b''

log = open(outpath, 'a')
def w(msg):
    log.write(msg + '\n'); log.flush()

r = RSP()
time.sleep(0.3)
p = r.recv_pkt(1)
if p is None:
    r.send('?'); p = r.recv_pkt(1)
w('initial %s' % p)
r.send('c')

base = None
va = None
armed = False
start = time.time()
hkpat = re.compile(rb'HKICK ([0-9a-f]{8}) ([0-9a-f]{8}) ([0-9a-f]{8})')
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
        hh = hkpat.findall(d)
        if hh:
            h = hh[0]
            va = int(h[1], 16)
            w('HKICK p=%s va=%08x pa=%s' % (h[0].decode(), va, h[2].decode()))
            st = r.interrupt()
            w('int %s' % st)
            resp = r.cmd('Z2,%x,4' % va, 2)
            w('Z2 %08x -> %s' % (va, resp))
            if resp != 'OK':
                resp = r.cmd('Z2,%x,2' % va, 2)
                w('Z2 try2 -> %s' % resp)
            armed = True
            r.send('c')
            continue
    pkt = r.recv_pkt(0.5)
    if not pkt:
        continue
    if pkt.startswith('T'):
        hits += 1
        g = r.cmd('g', 2)
        regs = None
        if g:
            try:
                raw = bytes.fromhex(g)
                regs = [int.from_bytes(raw[i*4:(i+1)*4], 'little') for i in range(16)]
            except Exception:
                regs = None
        if not regs:
            w('stop %s (no regs)' % pkt)
            r.send('c'); continue
        pc, lr, sp = regs[15], regs[14], regs[13]
        w('hit%d %s pc=%08x lr=%08x sp=%08x r0=%08x r1=%08x r2=%08x r3=%08x' % (
            hits, pkt, pc, lr, sp, regs[0], regs[1], regs[2], regs[3]))
        if base and base <= pc < base + 0x1400000:
            w('BUILDER pc=%08x off=%08x lr=%08x sp=%08x' % (pc, pc - base, lr, sp))
            w('code %s' % r.mem(pc, 96).hex())
            w('stack %s' % r.mem(sp, 192).hex())
            r.cmd('z2,%x,4' % va, 1)
            break
        if hits >= 100:
            w('too many hits'); break
        # advance past the faulting store, then resume with the watchpoint armed
        r.send('s')
        st = r.recv_pkt(2)
        w('step %s' % st)
        r.send('c')
    elif pkt.startswith('W'):
        w('exited %s' % pkt); break
w('END armed=%s base=%s va=%s hits=%d' % (armed, base and '%08x' % base, va and '%08x' % va, hits))
log.close()
