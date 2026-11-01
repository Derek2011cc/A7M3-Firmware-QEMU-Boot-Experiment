import socket, sys, time, re

addr = int(sys.argv[1], 16)
duration = float(sys.argv[2]) if len(sys.argv) > 2 else 240
maxstops = int(sys.argv[3]) if len(sys.argv) > 3 else 40
out = sys.argv[4] if len(sys.argv) > 4 else '/tmp/rsp_watch.log'

def csum(s):
    return sum(s.encode()) & 0xff

class RSP:
    def __init__(self, host='127.0.0.1', port=1234):
        self.s = None
        for _ in range(240):
            try:
                self.s = socket.create_connection((host, port), timeout=2)
                break
            except OSError:
                time.sleep(0.5)
        if self.s is None:
            raise SystemExit('connect failed')
        self.s.settimeout(2)
        self.buf = b''
    def send(self, pkt):
        self.s.sendall(('$%s#%02x' % (pkt, csum(pkt))).encode())
        # wait for ack
        while True:
            try:
                d = self.s.recv(1)
            except socket.timeout:
                return
            if not d:
                return
            if d == b'+':
                return
            if d == b'-':
                continue
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
            while b'$' in self.buf and b'#' in self.buf[self.buf.index(b'$'):]:
                i = self.buf.index(b'$')
                j = self.buf.index(b'#', i)
                if len(self.buf) < j + 3:
                    break
                pkt = self.buf[i+1:j].decode('latin1')
                self.buf = self.buf[j+3:]
                self.s.sendall(b'+')
                return pkt
        return None
    def cmd(self, pkt, timeout=2):
        self.send(pkt)
        return self.recv_pkt(timeout)
    def close(self):
        try:
            self.s.close()
        except Exception:
            pass

r = RSP()
time.sleep(0.5)
# drain greeting / query
p = r.recv_pkt(1)
r.send('?')
p = r.recv_pkt(1)
print('initial', p, flush=True)
r.send('Z2,%x,4' % addr)
print('watch resp', r.recv_pkt(2), flush=True)
log = open(out, 'a')
log.write('watch %08x\n' % addr)
stops = 0
start = time.time()
r.send('c')
while stops < maxstops and time.time() - start < duration:
    pkt = r.recv_pkt(3)
    if pkt is None:
        continue
    log.write('stop pkt %s\n' % pkt)
    if pkt.startswith('T') or pkt.startswith('S'):
        g = r.cmd('g', 2)
        if g:
            try:
                raw = bytes.fromhex(g)
                regs = [int.from_bytes(raw[i*4:(i+1)*4], 'little') for i in range(16)]
                log.write('PC=%08x r0=%08x r1=%08x r2=%08x r3=%08x r12=%08x lr=%08x\n' % (
                    regs[15], regs[0], regs[1], regs[2], regs[3], regs[12], regs[14]))
                log.flush()
            except Exception as e:
                log.write('regparse %s\n' % e)
        stops += 1
        r.send('c')
    elif pkt.startswith('W') or pkt.startswith('X'):
        break
print('stops', stops, flush=True)
log.close()
r.close()
