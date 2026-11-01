import subprocess, re
out = subprocess.run(['readelf', '-sW', '/home/derek/libObj.so'],
                     capture_output=True, text=True).stdout
rows = []
for line in out.splitlines():
    if 'ux6gfxeng' not in line:
        continue
    m = re.match(r'\s*\d+:\s+([0-9a-f]+)\s+(\d+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)', line)
    if not m:
        continue
    addr = int(m.group(1), 16)
    size = int(m.group(2))
    kind = m.group(3)
    name = m.group(7)
    rows.append((addr, size, kind, name))
for addr, size, kind, name in sorted(rows):
    print('%08x %5d %s %s' % (addr, size, kind, name))
