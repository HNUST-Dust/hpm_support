# SPDX-License-Identifier: Apache-2.0
"""Extract the target submission body unchanged for mocked admission checks."""
import pathlib
import sys
source = pathlib.Path(sys.argv[1]).read_text()
start = source.index('static int hpm_mcan_submit(')
brace = source.index('{', start)
depth = 1
end = brace + 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
pathlib.Path(sys.argv[2]).write_text(source[start:end] + '\n')
