#!/usr/bin/env python3
"""Install the user's pinned Turnip package as APK assets, with integrity checks."""
import argparse
import hashlib
import json
import shutil
from pathlib import Path
import struct
import zipfile

ZIP_SHA256 = '8c9fe5e357ddfe4970840cf91e2244b81e4dfbedf627d4f855db9f522ef7dd65'
DRIVER_SHA256 = 'ab879bc6605b81bc790eb3519608a998a681d0c47b2d957ea8219f2e6eaa6082'
DRIVER_NAME = 'vulkan.ad07xx.so'
ASSET_DIR = 'drivers/turnip-r7-sysmem'


def prepare(package, output):
    if hashlib.sha256(package.read_bytes()).hexdigest() != ZIP_SHA256:
        raise ValueError('ZIP does not match the supplied and upstream-verified Turnip R7 Sysmem')
    with zipfile.ZipFile(package) as archive:
        if sorted(archive.namelist()) != ['meta.json', DRIVER_NAME]:
            raise ValueError('Unexpected package contents')
        metadata_bytes = archive.read('meta.json')
        metadata = json.loads(metadata_bytes)
        driver = archive.read(DRIVER_NAME)
    if (metadata.get('schemaVersion') != 1 or metadata.get('libraryName') != DRIVER_NAME
            or not isinstance(metadata.get('minApi'), int) or metadata['minApi'] > 28):
        raise ValueError('Incompatible driver metadata')
    if (len(driver) != 17161105 or driver[:6] != b'\x7fELF\x02\x01'
            or struct.unpack_from('<HH', driver, 16) != (3, 183)
            or hashlib.sha256(driver).hexdigest() != DRIVER_SHA256):
        raise ValueError('Expected the exact ELF64 AArch64 shared driver')
    target = output / ASSET_DIR
    target.mkdir(parents=True, exist_ok=True)
    (target / DRIVER_NAME).write_bytes(driver)
    (target / 'meta.json').write_bytes(metadata_bytes)
    provenance = {'zip_sha256': ZIP_SHA256, 'driver_sha256': DRIVER_SHA256,
                  'driver_bytes': len(driver), 'metadata': metadata,
                  'upstream': 'https://github.com/K11MCH1/AdrenoToolsDrivers/releases/tag/v26.0.0-rc07',
                  'scope': 'Bundled driver; device compatibility and GS rendering not verified'}
    (target / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n')
    notices = Path(__file__).resolve().parents[2] / 'android' / 'third-party-notices'
    if notices.is_dir():
        shutil.copytree(notices, output / 'licenses' / 'ares-driver', dirs_exist_ok=True)
    return provenance


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--zip', type=Path, required=True)
    parser.add_argument('--assets', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare(args.zip, args.assets), indent=2))
