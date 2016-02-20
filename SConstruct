# -*- python -*-
# see LICENSE.md for license.
from glob import glob
from os.path import join as pathjoin

ccflags0 = ['-g', '-Wall', '-std=c++11']
ccflags1 = ['-g', '-Wall', '-flto', '-Ofast', '-fomit-frame-pointer', '-std=c++11']
env = Environment(CCFLAGS = ccflags0,
                  CPPDEFINES = [('_FILE_OFFSET_BITS', 64)],
                  CPPPATH = ['.'],
                  PROGSUFFIX = '.exe')
objs = map(lambda src: env.Object(src)[0], glob(pathjoin('densityxx', '*.cpp')))
env.Program('sharcxx', glob(pathjoin('sharcxx', '*.cpp')) + objs)
env.Program(pathjoin('tests', 'compile'), pathjoin('tests', 'compile.cpp'))
