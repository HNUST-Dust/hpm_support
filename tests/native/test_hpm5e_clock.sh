#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT
python3 - "$root/soc/hpmicro/HPM5E00/soc.c" "$build/hpm5e_clock.inc" <<'PY'
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text();a=s.index('static int soc_init_clock(');start=s.index('{',a);depth=1;end=start+1
while depth:
 depth+=(s[end]=='{')-(s[end]=='}');end+=1
Path(sys.argv[2]).write_text(s[a:end]+'\n')
PY
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
 -I "$build" "$root/tests/native/hpm5e_clock.c" -o "$build/clock"
"$build/clock"
