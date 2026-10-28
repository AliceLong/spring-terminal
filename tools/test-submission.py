#!/usr/bin/env python3
"""Run existing deterministic host tests without serial hardware or a model server."""
from pathlib import Path
import argparse,subprocess,sys,shutil,os,json,time
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--sanitize',action='store_true');a=p.parse_args()
build=R/'tests/.build';build.mkdir(parents=True,exist_ok=True)
reports=R/'validation';reports.mkdir(exist_ok=True)
(R/'evidence').mkdir(exist_ok=True)
cxx=os.environ.get('CXX') or shutil.which('clang++') or shutil.which('g++')
if not cxx:raise SystemExit('C++17 compiler required (clang++ or g++)')
board=R/'vendor/xiaozhi-esp32/main/boards/waveshare/esp32-s3-touch-amoled-2.16'
rows=[]
def run(label,cmd):
 start=time.monotonic();r=subprocess.run([str(x) for x in cmd],cwd=R,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
 clean=r.stdout.replace(str(R),'<PROJECT_ROOT>')
 (reports/(label+'.log')).write_text(clean)
 rows.append(dict(name=label,passed=r.returncode==0,seconds=round(time.monotonic()-start,2)))
 print(('PASS ' if r.returncode==0 else 'FAIL ')+label,flush=True)
 if r.returncode:
  print(clean[-4000:]);save();raise SystemExit(r.returncode)
 return r.stdout

def save():
 (reports/'host-tests.json').write_text(json.dumps(dict(tests=rows,passed=all(x['passed'] for x in rows),physical_hardware_tested=False,live_cloud_model_tested=False,excluded_tests=['migration_test.cc, full_save_test.cc and life_migration_test.cc require private legacy-device save input, intentionally not distributed']),indent=2))
for source in sorted(list((R/'tests/spring').glob('*test.cc'))+[R/'tests/spring/agency_flow_runner.cc']):
 name=source.stem
 if name in {'migration_test','full_save_test','life_migration_test'}:continue
 binary=build/name
 cmd=[cxx,'-std=c++17','-Wall','-Wextra','-Werror','-g','-I',R/'vendor/xiaozhi-esp32/main','-I',board,source,'-o',binary]
 if a.sanitize:cmd[1:1]=['-fsanitize=address,undefined']
 if name=='scenery_test':cmd.insert(cmd.index('-o'),board/'spring_scenery.cc')
 run('compile-'+name,cmd)
 args=[]
 if name=='character_test':args=[build/'v55-fixtures.json']
 if name=='agency_flow_runner':args=['fixture']
 if name=='story_flow_test':args=[build/'story-replay.json']
 out=run(name,[binary,*args])
 if name=='monitor_test':(R/'evidence/monitor-host-snapshot.json').write_text(out)
run('monitor-python',[sys.executable,'-m','unittest','discover','-s','monitor','-p','test_*.py','-v'])
run('mind-python',[sys.executable,'-m','unittest','discover','-s','robot-mind/tests','-v'])
node=shutil.which('node')
if node:
 for name in ['app.js','memory.js','roadshow.js']:run('syntax-'+name,[node,'--check',R/'monitor'/name])
 run('roadshow-reply-js',[node,R/'monitor/test_roadshow_reply.js'])
else:rows.append(dict(name='javascript',passed=True,skipped=True,reason='Node.js not installed'))
save();print('Existing host tests passed. No device was opened or flashed.')
