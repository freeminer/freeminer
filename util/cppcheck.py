import subprocess as s

import os,tempfile

includes = set()
checkers = []

def goodDir(d):
    if d == 'CMakeFiles' or d == '.git': return False

for top,ds,ns in os.walk('.'):
    ds[:] = [d for d in ds if goodDir(d)]
    for n in ns:
        base,ext = os.path.splitext(n)
        if ext[1:] in {'c','cc','cpp','h','hh','hpp'}:
            checkers.append(os.path.join(top,n))
            if ext[1:] in {'h','hh','hpp'}:
                includes.add(top)


o = tempfile.NamedTemporaryFile()
for include in includes:
    o.write((include+'\n').encode('utf-8'))
o.flush()
includes = o

o = tempfile.NamedTemporaryFile()
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
                   '--enable=all',
                   '--force'],
                  stdout=log,
                  stderr=xmllog)

while not os.path.exists(log.name):
    os.sleep(0.1)

with open(log.name) as inp:
    while checker.poll() is False:
        for line in inp.read().split('\n'):
            print('>',line)
        inp.seek(0,1)
