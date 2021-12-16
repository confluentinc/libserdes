#!/usr/bin/env python3
#

#
# Generate linker script to only expose symbols of the public API
#

import sys
import re


if __name__ == '__main__':

    funcs = list()
    last_line = ''

    for line in sys.stdin:
        m = re.match(r'^(\S+.*\s+\**)?(serdes_\S+)\s*\([^)]', line)
        if m:
            sym = m.group(2)
            # Ignore static (unused) functions
            m2 = re.match(r'(SERDES_UNUSED|__attribute__\(\(unused\)\))', last_line)
            if not m2:
                funcs.append(sym)
            last_line = ''
        else:
            last_line = line

    # Special symbols not covered by above matches or not exposed in
    # the public header files.
    #funcs.append('rd_ut_coverage_check')

    print('# Automatically generated by lds-gen.py - DO NOT EDIT')
    print('{\n global:')
    if len(funcs) == 0:
        print('    *;')
    else:
        for f in sorted(funcs):
            print('    %s;' % f)

        print('local:\n    *;')

    print('};')
