# -*- python -*-
# see LICENSE.md for license.
from glob import glob
from os.path import join as pathjoin

if ARGUMENTS.get('debug', 0): ccflags = ['-g', '-Wall', '-std=c++11']
else: ccflags = ['-O3', '-Wall', '-flto', '-Ofast', '-fomit-frame-pointer', '-std=c++11']
env = Environment(CCFLAGS = ccflags,
                  CPPDEFINES = [('_FILE_OFFSET_BITS', 64)],
                  CPPPATH = ['.'],
                  PROGSUFFIX = '.exe')
objs = map(lambda src: env.Object(src)[0], glob(pathjoin('densityxx', '*.cpp')))
env.Program('sharcxx', glob(pathjoin('sharcxx', '*.cpp')) + objs)
