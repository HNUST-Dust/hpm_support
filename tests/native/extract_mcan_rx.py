#!/usr/bin/env python3
from pathlib import Path
import sys
text = Path(sys.argv[1]).read_text()
def function(name):
    index = text.index(name + '(')
    start = text.rfind('static ', 0, index)
    brace = text.index('{', index)
    depth, end = 1, brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]
names = ['hpm_mcan_get_first_filter_index', 'hpm_mcan_get_message_from_rxfifo', 'hpm_mcan_drain_rxfifo', 'hpm_mcan_isr']
out = '\n'.join(function(name) for name in names)
for kind in ['std', 'ext']:
    needle = f'filter_{kind}->filter_mask = '
    start = text.index(needle)
    statement = text[start:text.index(';', start)+1]
    out += f'\nstatic uint32_t convert_{kind}_mask(const struct can_filter *filter) {{ mcan_filter_elem_t elem = {{0}}; mcan_filter_elem_t *filter_{kind} = &elem; {statement} return elem.filter_mask; }}\n'
Path(sys.argv[2]).write_text(out)
