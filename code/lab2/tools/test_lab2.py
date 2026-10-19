#!/usr/bin/env python3
"""Local semantic tests, preserved baseline checks and an actual QEMU RAM/allocator matrix."""
from pathlib import Path
import argparse, hashlib, json, re, struct, subprocess, time, resource, signal
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'evidence';OUT.mkdir(exist_ok=True)
checks=[]
settings={'qemu':'qemu-system-riscv64','bios':'firmware/opensbi-v0.4-qemu-virt-fdt81f00000.bin'}
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
def run(name,args,timeout=60):
 try:
  p=subprocess.run(args,cwd=ROOT,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=timeout)
 except subprocess.TimeoutExpired as exc:
  out=exc.stdout or ''
  if isinstance(out,bytes):out=out.decode('utf-8',errors='replace')
  p=subprocess.CompletedProcess(args,124,out+'\nLOCAL_TEST_TIMEOUT\n')
 (OUT/(name+'.log')).write_text(p.stdout)
 return p
def record(name,passed,**extra):
 checks.append({'name':name,'passed':bool(passed),**extra})
 print(('PASS ' if passed else 'FAIL ')+name,flush=True)
def fixtures():
 d=ROOT/'tests/fixtures';d.mkdir(exist_ok=True)
 for name in ['valid-2cells','valid-1cell','bad-magic','zero-memory','overflow-memory','bad-strings','bad-reg','bad-cells']:
  strings=b'#address-cells\0#size-cells\0device_type\0reg\0'
  def prop(n,v):return struct.pack('>III',3,len(v),strings.index(n.encode()+b'\0'))+v+b'\0'*((-len(v))%4)
  def node(n):
   x=n.encode()+b'\0';return struct.pack('>I',1)+x+b'\0'*((-len(x))%4)
  nc=1 if name=='valid-1cell' else 2
  cells=3 if name=='bad-cells' else nc
  base=0xfffffffffffff000 if name=='overflow-memory' else 0x80000000
  size=0 if name=='zero-memory' else 0x8000000
  reg=struct.pack('>II',base,size)if nc==1 else struct.pack('>QQ',base,size)
  if name=='bad-reg':reg=reg[:-1]
  body=node('')+prop('#address-cells',struct.pack('>I',cells))+prop('#size-cells',struct.pack('>I',nc))+node('memory@80000000')+prop('device_type',b'memory\0')+prop('reg',reg)+struct.pack('>III',2,2,9)
  so=56;st=so+len(body);total=st+len(strings)
  blob=bytearray(struct.pack('>10I',0xd00dfeed,total,so,st,40,17,16,0,len(strings),len(body))+bytes(16)+body+strings)
  if name=='bad-magic':blob[0]=0
  if name=='bad-strings':blob[12:16]=struct.pack('>I',total+1)
  (d/(name+'.dtb')).write_bytes(blob)
 return d
def check_preserved():
 p=ROOT/'kern/mm/best_fit_pmm.c';s=p.read_text()
 old=(ROOT/'provenance/best_fit_pmm.original.c').read_text()
 def span(t):
  a=t.index('static void\nbasic_check(void)');b=t.index('const struct pmm_manager best_fit_pmm_manager',a)
  return t[a:b].split('//这个结构体')[0].rstrip()
 record('baseline_checks_unchanged',span(s)==span(old))
def host_tests():
 d=fixtures();binary=ROOT/'tests/host-tests';
 args=['gcc','-std=gnu11','-g','-O1','-Wall','-Wextra','-Werror','-Wno-error=sign-compare','-Wno-error=unused-variable','-Wno-error=unused-but-set-variable','-fsanitize=address,undefined','-fno-omit-frame-pointer','-DLAB2_HOST_TEST','-Itests/host_include','-Ikern/mm','-Ikern/driver','-idirafter','libs','tests/test_allocators.c','kern/mm/best_fit_pmm.c','kern/mm/default_pmm.c','kern/mm/buddy_pmm.c','kern/mm/slub.c','kern/mm/pmm_layout.c','kern/driver/dtb.c','-o',str(binary)]
 p=run('host-build',args);record('host_build',p.returncode==0)
 if p.returncode:return
 p=run('host-tests',[str(binary),str(d)])
 record('host_semantics_and_sanitizers',p.returncode==0 and 'HOST_ALL_TESTS_PASSED' in p.stdout)
 for kind in ['buddy-size','buddy-double','slub-double','slub-interior']:
  p=run('reject-'+kind,[str(binary),str(d),kind])
  record('reject_'+kind,p.returncode == -signal.SIGABRT)
def qemu_tests():
 p=run('clean',['make','clean']);record('clean_build_tree',p.returncode==0)
 for allocator in ['best-fit','first-fit','buddy']:
  p=run('build-'+allocator,['make','-j2','PMM='+allocator,'SELFTEST=1'])
  record('build_'+allocator,p.returncode==0)
  if p.returncode:continue
  for mem in [32,64,128,256]:
   name=f'qemu-{allocator}-{mem}M'
   p=run(name,[settings['qemu'],'-machine','virt','-m',f'{mem}M','-nographic','-bios',settings['bios'],'-kernel','bin/ucore.img'],timeout=20)
   expected='0x%016x'%(mem*1024*1024)
   size=re.search(r'Size:\s*(0x[0-9a-fA-F]+)',p.stdout)
   end=re.search(r'managed end:\s*(0x[0-9a-fA-F]+)',p.stdout)
   passed=p.returncode==0 and 'LAB2_ALL_TESTS_PASSED' in p.stdout and 'MMU probe value: 0x2413460' in p.stdout and 'slub_check() succeeded!' in p.stdout and size and size.group(1)==expected and end and int(end.group(1),16)==0x80000000+min(mem,128)*1024*1024 and 'kernel panic' not in p.stdout
   record(name,passed,ram_mib=mem,allocator=allocator,exit_code=p.returncode)
 # Restore the submission's default Best-Fit image after switching modes.
 p=run('build-default',['make','-j2','PMM=best-fit','SELFTEST=1']);record('restore_default_best_fit',p.returncode==0)
def main():
 (OUT/'results.json').write_text(json.dumps({'passed':False,'status':'running'}))
 parser=argparse.ArgumentParser();parser.add_argument('--host-only',action='store_true')
 parser.add_argument('--qemu',default=settings['qemu']);parser.add_argument('--bios',default=settings['bios']);opt=parser.parse_args()
 settings.update(qemu=opt.qemu,bios=opt.bios)
 if not opt.host_only:
  version=subprocess.check_output([settings['qemu'],'--version'],cwd=ROOT,text=True).splitlines()[0]
  if version!='QEMU emulator version 4.1.1':
   raise SystemExit('This project validation requires QEMU 4.1.1; observed: '+version)
 check_preserved();host_tests()
 if not opt.host_only:qemu_tests()
 result={'kind':'local validation, not official 2026 grading','checks':checks,'passed':all(c['passed']for c in checks)}
 if not opt.host_only:
  result['qemu_version']=subprocess.check_output([settings['qemu'],'--version'],cwd=ROOT,text=True).splitlines()[0]
  result['bios']=settings['bios']
  result['bios_sha256']=hashlib.sha256((ROOT/settings['bios']).read_bytes()).hexdigest() if settings['bios']!='default' else None
 (OUT/'results.json').write_text(json.dumps(result,ensure_ascii=False,indent=2))
 raise SystemExit(0 if result['passed']else 1)
if __name__=='__main__':main()
