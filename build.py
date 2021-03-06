# Copyright (c) Mathias Kaerlev 2012.

# This file is part of Anaconda.

# Anaconda is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# Anaconda is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with Anaconda.  If not, see <http://www.gnu.org/licenses/>.

import sys
from setuptools import setup, Extension
from Cython.Distutils import build_ext
from Cython.Build import cythonize

from Cython.Compiler import Options
directive_defaults = Options.directive_defaults
Options.docstrings = False
if sys.argv[0].count('profile'):
    directive_defaults['profile'] = True

directive_defaults['cdivision'] = True
directive_defaults['infer_types'] = True
directive_defaults['auto_cpdef'] = True
directive_defaults['wraparound'] = False

ext_modules = []
libraries = []
include_dirs = ['./mmfparser/player']

names = open('names.txt', 'rb').read().splitlines()

for name in names:
    if name.startswith('#'):
        continue
    ext_modules.append(Extension(name, ['./' + name.replace('.', '/') + '.pyx'],
        include_dirs = include_dirs, language='c++'))

setup(
    name = 'mmfparser extensions',
    ext_modules = cythonize(ext_modules)
)
