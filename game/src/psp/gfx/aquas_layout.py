"""Fit the bounded Aquas extension while preserving all other production pins"""
import argparse
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--base', required=True)
parser.add_argument('--object', required=True)
parser.add_argument('--output', required=True)
args = parser.parse_args()
plan = json.loads(Path(args.base).read_text())
output = subprocess.check_output(['psp-nm', '-S', '--defined-only', args.object], text=True)
sizes = {p[3]: int(p[1], 16) for line in output.splitlines() if len(p := line.split()) == 4}
expected_sizes = {
    'psp_gfx_dl_run_internal.constprop.0.isra.0': {9788: 9820, 9760: 9796},
    'psp_gfx_dl_native_asset_ast_title_seg6_gfx_267B0': {2068: 2204},
    'psp_gfx_dl_handle_mtx.constprop.0.isra.0': {1424: 1400},
}
changes = []
for index, chunk in enumerate(plan['chunks']):
    if chunk['owner'] != 'src/psp/gfx/gfx_psp_dl.o':
        continue
    for function in chunk['functions']:
        actual = sizes.get(function['name'])
        if actual == function['size']:
            continue
        if actual != expected_sizes.get(function['name'], {}).get(function['size']):
            raise ValueError('unexpected Aquas code generation: ' + function['name'])
        extra = max(0, ((actual + 63) & ~63) - chunk['slot'])
        if extra:
            if extra > 192 or index == 0 or chunk['address'] - extra < (
                    plan['chunks'][index - 1]['address'] + plan['chunks'][index - 1]['slot']):
                raise ValueError('Aquas helper exceeds its preceding gap')
            chunk['address'] -= extra
            chunk['slot'] += extra
        changes.append((function['name'], function['size'], actual, extra))
        function['size'] = actual
Path(args.output).write_text(json.dumps(plan, indent=2) + '\n')
print('Aquas bounded layout:', changes)
