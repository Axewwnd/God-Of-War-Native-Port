#!/usr/bin/env python3
"""Reject incomplete generated runners; emit hashes for build provenance.

This checks source completeness, not game identity or gameplay correctness.
The linker remains authoritative for definitions, signatures and references.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    runtime = root / 'ps2xRuntime'
    required = [runtime / 'include' / name for name in
                ('ps2_recompiled_functions.h', 'ps2_recompiled_stubs.h')]
    registry = runtime / 'src/runner/register_functions.cpp'
    required.append(registry)
    errors = [f'Missing {p.relative_to(root)}' for p in required if not p.is_file()]
    sources = sorted((runtime / 'src/runner').glob('*.cpp'))
    generated = [p for p in sources if p.name != 'register_functions.cpp'
                 and re.search(r'void\s+\w+\s*\(\s*uint8_t\s*\*\s*rdram\s*,', p.read_text(errors='replace'))]
    if not generated:
        errors.append('No generated EE function implementations in ps2xRuntime/src/runner/*.cpp')
    # Every sub_/entry_ address function used by the registry must have a body.
    references = set(re.findall(r'\b(?:sub|entry)_\w+', registry.read_text())) if registry.is_file() else set()
    if not references:
        errors.append('No generated address functions referenced by the registration table')
    definitions = set()
    for path in sources + [p for p in required if p.suffix == '.h' and p.is_file()]:
        definitions.update(re.findall(r'\bvoid\s+((?:sub|entry)_\w+)\s*\([^;{}]*\)\s*\{', path.read_text(errors='replace')))
    missing = sorted(references - definitions)
    if missing:
        errors.append(f'{len(missing)} registry functions have no source definition; first: {missing[:8]}')
    files = sorted(set(p for p in required + sources if p.is_file()))
    report = {'status': 'failed' if errors else 'source-check-passed',
              'scope': 'Source presence and registry coverage only; not proof of a working game',
              'generated_translation_units': len(generated), 'registry_references': len(references),
              'missing_definitions': missing, 'errors': errors,
              'sha256': {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in files}}
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + '\n')
    for error in errors:
        print(error, file=sys.stderr)
    print(f'Generated EE translation units: {len(generated)}; registry references: {len(references)}')
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())
