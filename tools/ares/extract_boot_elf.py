#!/usr/bin/env python3
"""Read an ISO9660 image by HTTP ranges and extract the USA boot ELF only."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
from urllib.parse import urlsplit, urlunsplit
from urllib.request import Request, urlopen


class RangeReader:
    def __init__(self, url):
        if urlsplit(url).scheme != 'https':
            raise ValueError('An HTTPS URL is required')
        self.url = url
        self.offset = 0
        self.total = None
        self.requests = []

    def seek(self, offset):
        if offset < 0:
            raise ValueError('Negative ISO offset')
        self.offset = offset

    def read(self, size):
        if not 0 < size <= 16 * 1024 * 1024:
            raise ValueError(f'Invalid bounded range size: {size}')
        start, end = self.offset, self.offset + size - 1
        if self.total is not None and end >= self.total:
            raise ValueError('ISO range extends beyond the image')
        # Distinct URLs prevent intermediate caches reusing a different range.
        parts = urlsplit(self.url)
        query = parts.query + ('&' if parts.query else '') + f'ares_range={start}'
        url = urlunsplit((parts.scheme, parts.netloc, parts.path, query, ''))
        request = Request(url, headers={'Range': f'bytes={start}-{end}'})
        with urlopen(request, timeout=60) as response:
            match = re.fullmatch(r'bytes (\d+)-(\d+)/(\d+)', response.headers.get('Content-Range', ''))
            if response.status != 206 or not match or tuple(map(int, match.groups()[:2])) != (start, end):
                raise RuntimeError('Server did not honor the exact byte range; refusing an unbounded download')
            total = int(match.group(3))
            if self.total is not None and self.total != total:
                raise RuntimeError('Remote ISO size changed during extraction')
            self.total = total
            data = response.read(size + 1)
        if len(data) != size:
            raise RuntimeError(f'Expected {size} bytes; received {len(data)}')
        self.offset += size
        self.requests.append({'start': start, 'bytes': size, 'sha256': hashlib.sha256(data).hexdigest()})
        return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', required=True)
    parser.add_argument('--main-checkout', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    extractor = args.main_checkout / 'tools/iso_extract.py'
    spec = importlib.util.spec_from_file_location('ares_iso_extract', extractor)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    reader = RangeReader(args.url)
    files, layer1 = module.scan(reader)
    selected = [file for file in files if file[0].upper() in ('SCUS_973.99', 'SYSTEM.CNF')]
    if not any(name.upper() == 'SCUS_973.99' for name, _, _ in selected):
        raise RuntimeError('ISO does not contain the USA boot ELF SCUS_973.99')
    args.output.mkdir(parents=True, exist_ok=True)
    for name, lba, size in selected:
        reader.seek(lba * 2048)
        data = reader.read(size)
        (args.output / name.upper()).write_bytes(data)
        print(name, size, hashlib.sha256(data).hexdigest())
    manifest = {'iso_bytes': reader.total, 'layer1': layer1, 'files': files, 'requests': reader.requests}
    (args.output / 'remote-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')


if __name__ == '__main__':
    main()
