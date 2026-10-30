#!/usr/bin/env python3
"""Check actual APK contents for the pinned Turnip and its loading hooks."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import zipfile
from prepare_turnip import DRIVER_SHA256, DRIVER_NAME, ASSET_DIR


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('apk', type=Path)
    parser.add_argument('--build-tools', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    with zipfile.ZipFile(args.apk) as archive:
        driver = archive.read('assets/' + ASSET_DIR + '/' + DRIVER_NAME)
        if hashlib.sha256(driver).hexdigest() != DRIVER_SHA256:
            raise RuntimeError('Packaged Turnip differs from the supplied driver')
        hooks = ['lib/arm64-v8a/libmain_hook.so', 'lib/arm64-v8a/libhook_impl.so']
        for name in hooks:
            data = archive.read(name)
            if data[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<HH', data, 16) != (3, 183):
                raise RuntimeError(f'Hook is not an AArch64 shared object: {name}')
        if b'LauncherActivity' not in archive.read('classes.dex'):
            raise RuntimeError('Missing Java report/test screen')
        for notice in ['libadrenotools-BSD-2-Clause.txt', 'liblinkernsbypass-BSD-2-Clause.txt', 'Mesa-MIT.txt', 'Turnip-origin.txt']:
            if not archive.read('assets/licenses/ares-driver/' + notice):
                raise RuntimeError('Missing third-party notice: ' + notice)
    manifest = subprocess.check_output([str(args.build_tools / 'aapt'), 'dump', 'xmltree', str(args.apk), 'AndroidManifest.xml'], text=True)
    extraction = next((line for line in manifest.splitlines() if 'extractNativeLibs' in line), '')
    if '0xffffffff' not in extraction:
        raise RuntimeError('Native libraries must be extracted for adrenotools: ' + extraction)
    if 'com.ps2x.runner.LauncherActivity' not in manifest or ':runtime' not in manifest:
        raise RuntimeError('Missing launcher or isolated runtime process')
    report = {'status': 'turnip-assets-hooks-and-launcher-verified',
              'apk_sha256': hashlib.sha256(args.apk.read_bytes()).hexdigest(),
              'driver_sha256': DRIVER_SHA256, 'driver_bytes': len(driver), 'hooks': hooks,
              'scope': 'Packaging verification only; Turnip activation and GS rendering require device tests'}
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / 'turnip-apk-verification.json').write_text(json.dumps(report, indent=2) + '\n')
    (args.output / 'manifest.txt').write_text(manifest)
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
