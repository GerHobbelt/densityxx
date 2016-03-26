# -*- python -*-
# see LICENSE.md for license.
from glob import glob
from os.path import join as pathjoin

cppdefines = [('_FILE_OFFSET_BITS', 64)]
if ARGUMENTS.get('debug', 0):
    ccflags = ['-g', '-Wall', '-std=c++11']
    cppdefines.append('DENSITY_SHOW')
else:
    ccflags = ['-O3', '-Wall', '-flto', '-Ofast', '-fomit-frame-pointer', '-std=c++11']
env = Environment(CCFLAGS = ccflags,
                  CPPDEFINES = cppdefines,
                  CPPPATH = ['.'],
                  PROGSUFFIX = '.exe')
objs = map(lambda src: env.Object(src)[0], glob(pathjoin('densityxx', '*.cpp')))
env.Program('sharcxx', glob(pathjoin('sharcxx', '*.cpp')) + objs)
env.Program('showsz', 'showsz.cpp')
env.Object('compile', 'compile.cxx')
env.Program('test0', ['test0.cpp'] + objs)
env.Program('test3', ['test3.cpp'] + objs)
