import socket, sys, time, re

logpath = sys.argv[1]
duration = float(sys.argv[2]) if len(sys.argv) > 2 else 300
outpath = sys.argv[3] if len(sys.argv) > 3 else '/tmp/rsp_kick.log'

def csum(s):
    return sum(s.encode()) & 0xff

class RSP:
    def __init__(self):
        self.s = None
        for _ in range(600):
            try:
                self.s = socket.create_connection(('127.0.0.1', 1234), timeout=2)
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
                d = self.s.recv(4096)
            except socket.timeout:
                continue
            if not d:
                return None
            self.buf += d
            if b'+' in self.buf[:1] or b'-' in self.buf[:1]:
                self.buf = self.buf[1:]
            while b'$' in self.buf and b'#' in self.buf[self.buf.index(b'$'):]:
                i = self.buf.index(b'$')
                j = self.buf.index(b'#', i)
                if len(self.buf) < j + 3:
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

log = open(outpath, 'a')
def w(msg):
    log.write(msg + '\n')
    log.flush()

r = RSP()
time.sleep(0.5)
p = r.recv_pkt(1)
if p is None:
    r.send('?')
    p = r.recv_pkt(1)
w('initial %s' % p)
r.send('c')


watched = []
seen_lines = 0
pending = False
start = time.time()
hkpat = re.compile(rb'HKICK ([0-9a-f]{8}) ([0-9a-f]{8}) ([0-9a-f]{8})')
while time.time() - start < duration:
    # read new log data
    try:
        d = open(logpath, 'rb').read()
    except FileNotFoundError:
        d = b''
    hits = hkpat.findall(d)
    if len(hits) > seen_lines:
        for h in hits[seen_lines:]:
            va = int(h[1], 16)
            w('HKICK p=%s va=%08x pa=%s' % (h[0].decode(), va, h[2].decode()))
            if va and va not in watched and len(watched) < 2:
                # watch the next command buffers in the ring
                wva = va + 0x10
                st = r.interrupt()
                w('int stop %s' % st)
                resp = r.cmd('Z2,%x,1000' % wva, 2)
                w('Z2 %08x len 1000 -> %s' % (wva, resp))
                if resp != 'OK':
                    resp = r.cmd('Z2,%x,20' % wva, 2)
                    w('Z2 %08x len 20 -> %s' % (wva, resp))
                watched.append(wva)
                r.send('c')
        seen_lines = len(hits)
    pkt = r.recv_pkt(1)
    if pkt and (pkt.startswith('T') or pkt.startswith('S')):
        w('stop %s' % pkt)
        g = r.cmd('g', 2)
        if g:
            try:
                raw = bytes.fromhex(g)
                regs = [int.from_bytes(raw[i*4:(i+1)*4], 'little') for i in range(16)]
                w('PC=%08x r0=%08x r1=%08x r2=%08x r3=%08x r12=%08x lr=%08x' % (
                    regs[15], regs[0], regs[1], regs[2], regs[3], regs[12], regs[14]))
            except Exception as e:
                w('regparse %s' % e)
        r.send('c')
    elif pkt and (pkt.startswith('W') or pkt.startswith('X')):
        w('exited %s' % pkt)
        break
w('watched %s' % ['%08x' % a for a in watched])
log.close()
