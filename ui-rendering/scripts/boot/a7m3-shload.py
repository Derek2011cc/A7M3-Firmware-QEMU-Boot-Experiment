import os,socket,subprocess,time,sys
# Defaults match the development host; override with environment variables to
# run elsewhere. See scripts/boot/README.md.
D=os.environ.get("A7M3_DIR","/home/derek/firmware/a7m3")
Q=os.environ.get("A7M3_QEMU","/home/derek/qemu-ma1co/build-arm/arm-softmmu/qemu-system-arm")
LOGDIR=os.environ.get("A7M3_LOGDIR","/mnt/f/a7c2-re-main/a7m3-qemu-logs")
IMG=sys.argv[1]; TAG=sys.argv[2]; CMDFILE=sys.argv[3]; WAIT=float(sys.argv[4]) if len(sys.argv)>4 else 30
LOADF=sys.argv[5] if len(sys.argv)>5 else os.environ.get("A7M3_AVCAM","/home/derek/avcam403/av-cam.bin")
LOADA=sys.argv[6] if len(sys.argv)>6 else "0x035c6000"
os.makedirs(LOGDIR,exist_ok=True)
SOCK=f"/tmp/a7m3-{TAG}.sock"; LOG=f"{LOGDIR}/a7m3-{TAG}.log"; ERR=f"{LOGDIR}/a7m3-{TAG}.err"
try: os.unlink(SOCK)
except FileNotFoundError: pass
cmd=[Q,"-M","cxd90045","-kernel",f"{D}/vmlinux-console.bin","-initrd",f"{D}/initrd-qemu.img",
     "-drive",f"if=mtd,file={IMG},format=raw,snapshot=on","-serial",f"unix:{SOCK},server=on,wait=off",
     "-display",os.environ.get("A7M3_DISPLAY","none"),"-no-reboot",
     "-device",f"loader,file={LOADF},addr={LOADA},force-raw=on"]
if os.environ.get("A7M3_SEND_SO"):
    cmd += ["-device", f"loader,file={os.environ['A7M3_SEND_SO']},addr=0x10000000,force-raw=on"]
if os.environ.get("A7M3_QEMU_EXTRA"):
    cmd += os.environ["A7M3_QEMU_EXTRA"].split()
p=subprocess.Popen(cmd,stdout=subprocess.DEVNULL,stderr=open(ERR,"wb"))
for _ in range(100):
    if os.path.exists(SOCK): break
    time.sleep(0.1)
s=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM); s.connect(SOCK); s.settimeout(0.5)
buf=bytearray(); out=open(LOG,"wb")
def pump(sec):
    end=time.time()+sec
    while time.time()<end:
        try:
            d=s.recv(16384)
            if not d: return
            buf.extend(d); out.write(d); out.flush()
        except socket.timeout: pass
def wait(marker,timeout):
    end=time.time()+timeout
    while time.time()<end:
        pump(0.5)
        if marker in bytes(buf): return True
    return False
print("wait shell",flush=True); ok=wait(b"/ #",150); print("shell",ok,flush=True)
pump(WAIT)
if ok:
    for c in open(CMDFILE).read().splitlines():
        if not c.strip(): continue
        print("CMD",c,flush=True)
        s.sendall(c.encode()+b"\r\n")
        pump(float(os.environ.get("A7M3_PUMP","5")))
p.terminate(); time.sleep(0.5)
try: p.kill()
except Exception: pass
print("log",LOG,flush=True)
