import os, socket, subprocess, time, sys, re
# Defaults match the development host; override with environment variables.
D = os.environ.get("A7M3_DIR", "/home/derek/firmware/a7m3")
Q = os.environ.get("A7M3_QEMU", "/home/derek/qemu-ma1co/build-arm/arm-softmmu/qemu-system-arm")
LOGDIR = os.environ.get("A7M3_LOGDIR", "/mnt/f/a7c2-re-main/a7m3-qemu-logs")
IMG = sys.argv[1]
TAG = sys.argv[2]
CMDS = sys.argv[3:]
LOGFILE = None
if CMDS and CMDS[0].startswith("@"):
    LOGFILE = CMDS[0][1:]
    CMDS = [l for l in open(LOGFILE).read().splitlines() if l.strip()]
WAIT = float(os.environ.get("A7M3_WAIT", "20"))
os.makedirs(LOGDIR, exist_ok=True)
SOCK = "/tmp/a7m3-%s.sock" % TAG
LOG = "%s/a7m3-%s.log" % (LOGDIR, TAG)
ERR = "%s/a7m3-%s.err" % (LOGDIR, TAG)
try:
    os.unlink(SOCK)
except FileNotFoundError:
    pass
cmd = [Q, "-M", "cxd90045", "-kernel", D + "/vmlinux-console.bin",
       "-initrd", D + "/initrd-qemu.img",
       "-drive", "if=mtd,file=%s,format=raw,snapshot=on" % IMG,
       "-serial", "unix:%s,server=on,wait=off" % SOCK,
       "-display", "none", "-no-reboot"]
p = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=open(ERR, "wb"))
for _ in range(100):
    if os.path.exists(SOCK):
        break
    time.sleep(0.1)
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect(SOCK)
s.settimeout(0.5)
buf = bytearray()
out = open(LOG, "wb")

def pump(sec):
    end = time.time() + sec
    while time.time() < end:
        try:
            d = s.recv(65536)
            if not d:
                return
            buf.extend(d)
            out.write(d)
            out.flush()
        except socket.timeout:
            pass

def waittext(txt, timeout):
    end = time.time() + timeout
    while time.time() < end:
        pump(0.5)
        if txt.encode() in bytes(buf[-200000:]):
            return True
    return False

def run(tag, cmdline, timeout=30):
    s.sendall(("echo BEGIN_%s_$$; " % tag + cmdline + "; echo END_%s_$$" % tag).encode() + b"\r\n")
    ok = waittext("END_%s_" % tag, timeout)
    pump(0.3)
    t = bytes(buf).decode('latin1', 'replace')
    bm = list(re.finditer(r"BEGIN_%s_(\d+)" % re.escape(tag), t))
    em = list(re.finditer(r"END_%s_(\d+)" % re.escape(tag), t))
    if not bm or not em:
        return None
    return t[bm[-1].end():em[-1].start()]

print("shell", waittext("/ #", 90), flush=True)
pump(WAIT)
for i, c in enumerate(CMDS):
    seg = run("c%d" % i, c, int(os.environ.get("A7M3_CMD_TIMEOUT", "60")))
    print("===== CMD %d: %s" % (i, c), flush=True)
    print(seg if seg is not None else "<no match>", flush=True)
s.sendall(b"\x01x")
time.sleep(0.5)
p.terminate()
time.sleep(0.5)
try:
    p.kill()
except Exception:
    pass
print("log", LOG, flush=True)
