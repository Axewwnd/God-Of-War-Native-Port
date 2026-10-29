#!/usr/bin/env python3
"""Verify that an APK actually contains ARM64 code for every registered game function.

This verifies packaging and linked symbols, not boot/rendering/gameplay.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('apk', type=Path)
    parser.add_argument('--llvm-bin', type=Path, required=True)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    registry = args.root / 'ps2xRuntime/src/runner/register_functions.cpp'
    references = set(re.findall(r'\b(?:sub|entry)_\w+', registry.read_text()))
    if not references:
        raise RuntimeError('Empty or missing generated game registry')
    with zipfile.ZipFile(args.apk) as archive:
        names = archive.namelist()
        if 'AndroidManifest.xml' not in names:
            raise RuntimeError('APK is missing AndroidManifest.xml')
        native = [name for name in names if name.startswith('lib/') and name.endswith('.so')]
        if any(not name.startswith('lib/arm64-v8a/') for name in native):
            raise RuntimeError(f'Unexpected native ABI: {native}')
        library = archive.read('lib/arm64-v8a/libps2EntryRunner.so')
    if (library[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<H', library, 16)[0] != 3
            or struct.unpack_from('<H', library, 18)[0] != 183):
        raise RuntimeError('Runner is not a shared ELF64 little-endian AArch64 library')
    library_path = args.output / 'libps2EntryRunner.so'
    library_path.write_bytes(library)
    symbols = subprocess.check_output([str(args.llvm_bin / 'llvm-nm'), '-D', '--defined-only', '--demangle', str(library_path)], text=True)
    (args.output / 'defined-symbols.txt').write_text(symbols)
    definitions = set(re.findall(r'\b((?:sub|entry)_\w+)\(', symbols))
    missing = sorted(references - definitions)
    if missing:
        raise RuntimeError(f'{len(missing)} game functions absent from packaged library: {missing[:8]}')
    if not re.search(r'\bT ANativeActivity_onCreate$', symbols, re.MULTILINE):
        raise RuntimeError('Missing defined NativeActivity entry point')
    if not re.search(r'\b[BDSR] g_ps2RecompiledFunctionTable$', symbols, re.MULTILINE):
        raise RuntimeError('Missing linked recompiled function table')
    report = {'status': 'apk-structure-and-game-symbols-verified',
              'scope': 'Packaging and linked code only; gameplay and rendering not tested',
              'apk': args.apk.name, 'apk_bytes': args.apk.stat().st_size,
              'apk_sha256': hashlib.sha256(args.apk.read_bytes()).hexdigest(),
              'runner_bytes': len(library), 'runner_sha256': hashlib.sha256(library).hexdigest(),
              'elf_machine': 'AArch64', 'registry_function_count': len(references),
              'linked_game_function_count': len(definitions), 'missing_functions': missing,
              'native_libraries': native}
    (args.output / 'apk-verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
