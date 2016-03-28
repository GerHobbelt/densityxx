# -*- python -*-
# see LICENSE.md for license.
from glob import glob
from os.path import join as pathjoin

cppdefines = [('_FILE_OFFSET_BITS', 64)]
linkflags = []
if ARGUMENTS.get('debug', 0):
    ccflags = ['-g', '-Wall', '-std=c++11']
    cppdefines.append('DENSITY_SHOW')
    cppdefines.append(('DENSITY_INLINE', ''))
else:
    ccflags = ['-O3', '-Wall', '-flto', '-Ofast', '-std=c++11']
    cppdefines.append(('DENSITY_INLINE', 'inline __attribute__((always_inline))'))

if ARGUMENTS.get('prof', 0):
    ccflags.extend(['-pg', '-fprofile-arcs', '-ftest-coverage'])
    linkflags.extend(['-pg', '-fprofile-arcs', '-ftest-coverage'])
else:
    ccflags.append('-fomit-frame-pointer')

env = Environment(CCFLAGS = ccflags,
                  CPPDEFINES = cppdefines,
                  CPPPATH = ['.'],
                  LINKFLAGS = linkflags,
                  PROGSUFFIX = '.exe')
objs = map(lambda src: env.Object(src)[0], glob(pathjoin('densityxx', '*.cpp')))
env.Program('sharcxx', glob(pathjoin('sharcxx', '*.cpp')) + objs)
env.Program('showsz', 'showsz.cpp')
env.Object('compile', 'compile.cxx')
