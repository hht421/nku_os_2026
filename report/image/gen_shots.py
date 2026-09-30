# -*- coding: utf-8 -*-
import html, re, os

BASE = "/mnt/c/Users/11650/Desktop/操作系统/实验/nku_os_2026/report/image"
os.makedirs(BASE + "/html", exist_ok=True)

CSS = """
body{margin:0;background:#202124;display:flex;justify-content:center;padding:24px;font-family:'Cascadia Mono','Consolas',monospace}
.term{width:1080px;background:#0c0c0c;border-radius:10px;box-shadow:0 8px 30px rgba(0,0,0,.5);overflow:hidden;border:1px solid #3c3c3c}
.bar{background:#2d2d2d;padding:9px 14px;display:flex;align-items:center;gap:8px}
.dot{width:12px;height:12px;border-radius:50%}
.r{background:#ff5f57}.y{background:#febc2e}.g{background:#28c840}
.title{color:#9a9a9a;font-size:12.5px;margin-left:10px}
.body{padding:14px 18px 18px;font-size:13px;line-height:1.5;color:#d4d4d4;white-space:pre-wrap;word-break:break-all}
.cmd{color:#4ec9b0;font-weight:bold}
.cc{color:#6a9955}
.key{color:#569cd6;font-weight:bold}
.ok{color:#4ec9b0;font-weight:bold}
.brk{color:#c586c0}
.reg{color:#9cdcfe}
.addr{color:#ce9178}
.banner{color:#808080}
"""

def esc(s):
    return html.escape(s)

def colorize(lines):
    out = []
    for ln in lines:
        e = esc(ln)
        c = None
        if ln.startswith("$ "):
            c = "cmd"
        elif ln.startswith("+ cc") or ln.startswith("+ ld") or "objcopy" in ln:
            c = "cc"
        elif "THU.CST" in ln:
            c = "ok"
        elif ln.startswith("===") or re.search(r"\[\d\]", ln):
            c = "key"
        elif ln.startswith("Breakpoint") or "kern_entry ()" in ln or "in ?? ()" in ln:
            c = "brk"
        elif re.match(r"^(pc|sp|ra|a0|a1|gp|tp|t\d|fp|s\d+|x\d+)\s", ln):
            c = "reg"
        elif "=> 0x" in ln or ln.startswith("   0x"):
            c = "addr"
        elif ("OpenSBI v" in ln or "Platform" in ln or "Boot HART" in ln
              or "Domain0" in ln or "PMP" in ln or "MIDELEG" in ln or "MEDELEG" in ln):
            c = "banner"
        out.append('<span class="%s">%s</span>' % (c, e) if c else e)
    return "\n".join(out)

def page(fname, title, content):
    doc = ('<!DOCTYPE html><html><head><meta charset="utf-8"><style>%s</style></head>'
           '<body><div class="term"><div class="bar">'
           '<div class="dot r"></div><div class="dot y"></div><div class="dot g"></div>'
           '<div class="title">%s</div></div>'
           '<div class="body">%s</div></div></body></html>') % (CSS, esc(title), content)
    with open(BASE + "/html/" + fname, "w", encoding="utf-8") as f:
        f.write(doc)

# shot 1: make clean && make qemu
with open(BASE + "/make_qemu_raw.log", encoding="utf-8", errors="replace") as f:
    qlog = f.read().splitlines()
qlog = [l for l in qlog if l.strip() and "terminating on signal" not in l]
page("shot1.html", "ubuntu-wsl:~/nku_os_2026/code/lab1 — make clean && make qemu", colorize(qlog))

# gdb trace: split
with open(BASE + "/gdb_trace_raw.log", encoding="utf-8", errors="replace") as f:
    glog = f.read().splitlines()
glog = [l for l in glog if l.strip()]
idx_b = next(i for i, l in enumerate(glog) if l.startswith("Breakpoint 2 at"))
partA, partB = glog[:idx_b], glog[idx_b:]
ghead = ["$ riscv64-unknown-elf-gdb -batch -ex 'file bin/kernel' -ex 'set arch riscv:rv64' \\",
         "    -ex 'target remote localhost:1234' ...   # 另一窗格: make debug"]
page("shot2.html", "ubuntu-wsl:~/nku_os_2026/code/lab1 — riscv64-unknown-elf-gdb (练习2: 上电→OpenSBI)", colorize(ghead + partA))
page("shot3.html", "ubuntu-wsl:~/nku_os_2026/code/lab1 — riscv64-unknown-elf-gdb (练习2: 内核入口→kern_init)", colorize(partB))
print("OK", sorted(os.listdir(BASE + "/html")))
