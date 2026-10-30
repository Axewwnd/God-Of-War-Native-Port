#!/usr/bin/env python3
"""Exercise the pinned package import and reject damaged/unexpected inputs."""
import argparse
import hashlib
import json
from pathlib import Path
import tempfile
from unittest.mock import patch
import zipfile

import prepare_turnip as importer


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--zip', type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        report = importer.prepare(args.zip, root / 'assets')
        installed = root / 'assets' / importer.ASSET_DIR / importer.DRIVER_NAME
        assert hashlib.sha256(installed.read_bytes()).hexdigest() == importer.DRIVER_SHA256
        assert report['driver_bytes'] == installed.stat().st_size

        damaged = root / 'damaged.zip'
        damaged.write_bytes(args.zip.read_bytes() + b'tampered')
        try:
            importer.prepare(damaged, root / 'rejected')
            raise AssertionError('Damaged ZIP accepted')
        except ValueError:
            assert not (root / 'rejected').exists()

        with zipfile.ZipFile(args.zip) as archive:
            metadata = archive.read('meta.json')
            driver = archive.read(importer.DRIVER_NAME)

        def reject_fixture(name, entries):
            fixture = root / name
            with zipfile.ZipFile(fixture, 'w') as archive:
                for filename, content in entries.items():
                    archive.writestr(filename, content)
            # Bypass only the outer hash so inner validation gets exercised.
            with patch.object(importer, 'ZIP_SHA256', hashlib.sha256(fixture.read_bytes()).hexdigest()):
                try:
                    importer.prepare(fixture, root / name.replace('.zip', ''))
                    raise AssertionError(f'{name} accepted')
                except ValueError:
                    pass

        reject_fixture('path-traversal.zip', {'../escape.so': driver, 'meta.json': metadata})
        bad_meta = json.loads(metadata)
        bad_meta['libraryName'] = '../escape.so'
        reject_fixture('bad-metadata.zip', {importer.DRIVER_NAME: driver, 'meta.json': json.dumps(bad_meta)})
        wrong_abi = bytearray(driver)
        wrong_abi[18:20] = (62).to_bytes(2, 'little')
        reject_fixture('wrong-abi.zip', {importer.DRIVER_NAME: wrong_abi, 'meta.json': metadata})
        truncated = driver[:-1]
        reject_fixture('truncated.zip', {importer.DRIVER_NAME: truncated, 'meta.json': metadata})
    print('PASS: exact import; damaged archive, traversal, metadata, ABI and truncation rejected')


if __name__ == '__main__':
    main()
