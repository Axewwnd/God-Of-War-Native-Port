#!/usr/bin/env python3
"""Generate the USA game sources from the owner's extracted PS2 executable.

No game code is synthesized when the original executable is missing.
Output is local: generated/, ares-game-sources/{include,runner}, provenance.json.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf', type=Path, required=True)
    parser.add_argument('--recompiler', type=Path, required=True)
    parser.add_argument('--main-checkout', type=Path, required=True)
    parser.add_argument('--reference-checkout', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    elf = args.elf.resolve()
    recomp = args.recompiler.resolve()
    main_repo = args.main_checkout.resolve()
    reference = args.reference_checkout.resolve()
    output = args.output.resolve()
    template = reference / 'config/recomp.template.toml'
    funcmap = reference / 'config/funcmap.csv'
    overrides = main_repo / 'port/overrides-usa/gow_overrides.cpp'
    headers = [reference / 'src/gow_pad2_packet.h', main_repo / 'port/switch/gow_gs_replay.h']
    for path in [elf, recomp, template, funcmap, overrides] + headers:
        if not path.is_file():
            parser.error(f'Required file not found: {path}')
    with elf.open('rb') as stream:
        header = stream.read(24)
    if (elf.name != 'SCUS_973.99' or len(header) < 24 or header[:6] != b'\x7fELF\x01\x01'
            or struct.unpack_from('<H', header, 18)[0] != 8):
        parser.error('Expected extracted NTSC-U SCUS_973.99, ELF32 little-endian MIPS. Region/hash still require verification.')
    if output.exists() and any(output.iterdir()):
        parser.error('Output must be empty; use a new directory to avoid mixing generated revisions.')
    output.mkdir(parents=True, exist_ok=True)
    generated = output / 'generated'
    text = template.read_text()
    for key, path in {'ELF': elf, 'MAP': funcmap, 'OUT': generated}.items():
        text = text.replace('"@' + key + '@"', json.dumps(str(path)))
    config = output / 'recomp.toml'
    config.write_text(text)
    with (output / 'recompile.log').open('w') as log:
        subprocess.run([str(recomp), str(config)], stdout=log, stderr=subprocess.STDOUT, check=True)
    bundle = output / 'ares-game-sources'
    for name in ('include', 'runner'):
        (bundle / name).mkdir(parents=True)
    for path in generated.glob('*.h'):
        shutil.copy2(path, bundle / 'include' / path.name)
    for path in generated.glob('*.cpp'):
        shutil.copy2(path, bundle / 'runner' / path.name)
    shutil.copy2(overrides, bundle / 'runner' / overrides.name)
    for path in headers:
        shutil.copy2(path, bundle / 'include' / path.name)
    subprocess.run([sys.executable, str(Path(__file__).with_name('adapt_game_sources.py')),
                    '--include', str(bundle / 'include'), '--runner', str(bundle / 'runner')], check=True)
    provenance = {'input_sha256': {str(p): sha256(p) for p in [elf, recomp, template, funcmap, config, overrides] + headers},
                  'repositories': {str(p): subprocess.check_output(['git', '-C', str(p), 'rev-parse', 'HEAD'], text=True).strip()
                                   for p in (main_repo, reference)},
                  'bundle_sha256': {str(p.relative_to(bundle)): sha256(p) for p in sorted(bundle.rglob('*')) if p.is_file()},
                  'status': 'generated; compilation and gameplay are not verified'}
    (output / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n')
    print(f'Generated local source bundle: {bundle}')
    print('Install into a clean runtime checkout and run check_game_sources.py before compiling.')


if __name__ == '__main__':
    main()
