"""Validate every generated shader under both Vulkan environments, preserving exact errors."""
import hashlib
import json
import pathlib
import subprocess
import sys
validator, directory, output = sys.argv[1:]
files = sorted(pathlib.Path(directory).glob('*.spv'))
if len(files) != 136:
    raise SystemExit(f'Expected 136 real GS shaders, got {len(files)}')
report = {'shader_count': len(files), 'gpu_execution_tested': False, 'shaders': []}
for path in files:
    entry = {'file': path.name, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
    for env in ['vulkan1.0', 'vulkan1.1']:
        run = subprocess.run([validator, '--target-env', env, str(path)], text=True, capture_output=True)
        entry[env] = {'exit_code': run.returncode, 'diagnostic': (run.stdout + run.stderr).strip()}
    report['shaders'].append(entry)
report['passed'] = {env: sum(s[env]['exit_code'] == 0 for s in report['shaders']) for env in ['vulkan1.0', 'vulkan1.1']}
pathlib.Path(output).write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({key: value for key, value in report.items() if key != 'shaders'}, indent=2))
