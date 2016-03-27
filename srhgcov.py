#!/usr/bin/python
from glob import glob

def srhfile(rfpath):
    title = False
    for ln in open(rfpath, 'rt'):
        numstr = ln.split(':')[0].strip()
        if numstr in ['-', '$$$$$', '%%%%%', '#####']: continue
        num = int(numstr)
        if not title:
            print(rfpath)
            title = True
        if num > 1024: prefix = '****'
        else: prefix = '....'
        print(prefix + ln[:-1])

map(lambda fname: srhfile(fname), glob('*.gcov'))
