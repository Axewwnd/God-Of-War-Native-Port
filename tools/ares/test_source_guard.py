"""Negative regressions: an incomplete game must never pass source preflight."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

CHECK = Path(__file__).with_name('check_game_sources.py')


class SourceGuardTests(unittest.TestCase):
    def test_checkout_without_generated_sources_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            result = subprocess.run([sys.executable, str(CHECK), '--root', tmp], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Missing', result.stderr)

    def test_registry_and_headers_without_function_bodies_are_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            runtime = Path(tmp) / 'ps2xRuntime'
            (runtime / 'include').mkdir(parents=True)
            runner = runtime / 'src/runner'
            runner.mkdir(parents=True)
            for name in ('ps2_recompiled_functions.h', 'ps2_recompiled_stubs.h'):
                (runtime / 'include' / name).touch()
            (runner / 'register_functions.cpp').write_text('table[0] = sub_00100008_0x100008;')
            result = subprocess.run([sys.executable, str(CHECK), '--root', tmp], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('No generated EE function implementations', result.stderr)
            self.assertIn('sub_00100008_0x100008', result.stderr)

    def test_partial_function_set_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            runtime = Path(tmp) / 'ps2xRuntime'
            (runtime / 'include').mkdir(parents=True)
            runner = runtime / 'src/runner'
            runner.mkdir(parents=True)
            for name in ('ps2_recompiled_functions.h', 'ps2_recompiled_stubs.h'):
                (runtime / 'include' / name).touch()
            (runner / 'register_functions.cpp').write_text('table[0] = sub_a; table[1] = sub_b;')
            (runner / 'a.cpp').write_text('void sub_a(uint8_t* rdram, R5900Context* ctx, PS2Runtime* runtime) {}')
            result = subprocess.run([sys.executable, str(CHECK), '--root', tmp], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('sub_b', result.stderr)


if __name__ == '__main__':
    unittest.main()
