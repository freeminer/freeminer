import subprocess as s

import os,tempfile,time

includes = set()
checkers = []

def goodDir(d):
    if d == 'CMakeFiles' or d == '.git': return False
    return True

for top,ds,ns in os.walk('.'):
    ds[:] = [d for d in ds if goodDir(d)]
    for n in ns:
        base,ext = os.path.splitext(n)
        ext = ext[1:]
        if ext in {'c','cc','cpp','h','hh','hpp'}:
            checkers.append(os.path.join(top,n))
            if ext in {'h','hh','hpp'}:
                includes.add(top)


o = tempfile.NamedTemporaryFile()
for include in includes:
    o.write((include+'\n').encode('utf-8'))
o.flush()
includes = o

#o = tempfile.NamedTemporaryFile()
o = open('.checkers.tmp','w+b')
for path in checkers:
    o.write((path+'\n').encode('utf-8'))
o.flush()
checkers = o

log = open('cppcheck.log','wt')
xmllog = open('cppcheck.log.xml','wt')
checker = s.Popen(['cppcheck',
                   '--file-list='+checkers.name,
                   '--includes-file='+includes.name,
                   '--inconclusive',
                   '--inline-suppr',
                   '-j','3',
                   '--xml',
                   '--xml-version=2',
                   '--enable=all',
                   '--force'],
                  stdout=log,
                  stderr=xmllog)

while not os.path.exists(log.name):
    os.sleep(0.1)
print('found log file, begin tee hack',checker.poll())
with open(log.name) as inp:
    buf = ''
    while True:
        buf += inp.read()
        if buf:
            lines = buf.split('\n')
            buf = lines[-1]
            for line in lines[:-1]:
                print('> ',line)
            inp.seek(0,1)
        time.sleep(0.1)
        if checker.poll() is not None: break
print('done')
