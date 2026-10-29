#!/usr/bin/env python3
"""Adapt the pinned hand-written GoW overrides to this runtime and Android STL.

Does not alter the generated EE functions, stubs, or registration table.
"""
import argparse
from pathlib import Path


def replace_once(text, old, new):
    if new in text:
        return text
    if text.count(old) != 1:
        raise RuntimeError('Unexpected upstream source revision; compatibility edit needs review')
    return text.replace(old, new, 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--include', dest='include_dir', type=Path, required=True)
    parser.add_argument('--runner', type=Path, required=True)
    args = parser.parse_args()
    header = args.include_dir / 'gow_gs_replay.h'
    source = args.runner / 'gow_overrides.cpp'
    text = header.read_text()
    text = replace_once(text, '#include <charconv>', '#include "ares_android_compat.h"')
    text = replace_once(text,
        '            const char *end=text+std::strlen(text);\n'
        '            const auto result=std::from_chars(text,end,parsed);\n'
        '            if(result.ec!=std::errc{} || result.ptr!=end || !std::isfinite(parsed) ||',
        '            if(!ares::parseFiniteDecimal(text,parsed) ||')
    if text != header.read_text():
        header.write_text(text)
    text = source.read_text()
    text = replace_once(text, '#include "runtime/ee_scheduler.h"',
                        '#include "runtime/ee_scheduler.h"\n#include "ares_android_compat.h"')
    text = replace_once(text, 'runtime->eeScheduler().advanceQuietRepeated(7u, pending - 1u)',
                        'ares::advanceQuietRepeated(runtime->eeScheduler(), 7u, pending - 1u)')
    if text != source.read_text():
        source.write_text(text)
    print('Adapted decimal parsing and optional countdown optimization; generated game code unchanged.')


if __name__ == '__main__':
    main()
