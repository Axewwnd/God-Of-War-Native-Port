#!/usr/bin/env python3
"""Prove that the verifier rejects an ARM64 runtime substituted for the game.

The negative fixture is temporary, is never signed/published, and is deleted.
"""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--apk', type=Path, required=True)
parser.add_argument('--llvm-bin', type=Path, required=True)
parser.add_argument('--runtime-library', type=Path, required=True)
args = parser.parse_args()
with tempfile.TemporaryDirectory() as tmp:
    directory = Path(tmp)
    fixture = directory / 'runtime-only-fixture.zip'
    with zipfile.ZipFile(args.apk) as original:
        manifest = original.read('AndroidManifest.xml')
    with zipfile.ZipFile(fixture, 'w') as archive:
        archive.writestr('AndroidManifest.xml', manifest)
        archive.write(args.runtime_library, 'lib/arm64-v8a/libps2EntryRunner.so')
    result = subprocess.run([sys.executable, str(Path(__file__).with_name('verify_apk.py')),
                             str(fixture), '--llvm-bin', str(args.llvm_bin),
                             '--output', str(directory / 'verification')], capture_output=True, text=True)
    if result.returncode == 0 or 'game functions absent from packaged library' not in result.stderr:
        raise RuntimeError('Runtime-only fixture was not rejected for missing game functions: ' + result.stderr)
    print('PASS: substituting an ARM64 runtime for the game library is rejected; temporary fixture removed.')
