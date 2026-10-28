#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Compile real ESC transfer functions; replace only volatile MMIO with bus probes."""
from pathlib import Path
import re
import sys
root=Path(__file__).resolve().parents[2]
source=(root/'drivers/ethercat/esc_hpmicro.c').read_text()
parts=[]
for name in ('esc_pdram_word_access','esc_read_words','esc_write_words','esc_read_block','esc_read','esc_write'):
    m=re.search(r'^static (?:void|bool) '+name+r'\([^;]*?\)\s*\{',source,re.M)
    if not m: raise RuntimeError(name)
    start=source.index('{',m.start())
    end=start+1; depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    body=source[m.start():end]
    # Write substitutions before read substitutions, to preserve bus direction.
    body=re.sub(r'\*\(volatile uint(8|16|32)_t \*\)\((addr \+ (?:pos|i))\) = ([^;]+);',r'bus_write(\2, \1 / 8, \3);',body)
    body=re.sub(r'\*\(volatile uint(8|16|32)_t \*\)addr = ([^;]+);',r'bus_write(addr, \1 / 8, \2);',body)
    body=re.sub(r'\*\(volatile uint(8|16|32)_t \*\)\((addr \+ (?:pos|i))\)',r'bus_read(\2, \1 / 8)',body)
    body=re.sub(r'\*\(volatile uint(8|16|32)_t \*\)addr',r'bus_read(addr, \1 / 8)',body)
    if 'volatile' in body: raise RuntimeError('unhandled MMIO in '+name)
    if name in ('esc_pdram_word_access','esc_read_words','esc_write_words'):
        body='#if defined(CONFIG_HPM_ESC_PDRAM_WORD_ACCESS)\n'+body+'\n#endif'
    parts.append(body)
Path(sys.argv[1]).write_text('\n\n'.join(parts)+'\n')
