#!/usr/bin/env python3
from pathlib import Path
import sys
text = Path(sys.argv[1]).read_text()
def function(name):
    start = text.index('const struct hpm_mcan_rx_timing *' + name) if name == 'hpm_mcan_last_rx_timing' else text.index('static void ' + name)
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]
config_start = text.index('    mcan_get_default_config(can, config);')
config_end = text.index('    clock_add_to_group', config_start)
config = text[config_start:config_end]
out = function('hpm_mcan_last_rx_timing') + '\n#if defined(CONFIG_HPM_MCAN_RX_TIMING)\n' + function('hpm_mcan_sample_rx_timing') + '\n#endif\n'
out += 'static void configure_timing(mcan_config_t *config) { void *can = NULL;\n' + config + '}\n'
Path(sys.argv[2]).write_text(out)
