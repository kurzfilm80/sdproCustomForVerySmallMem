#!/usr/bin/env python3
"""Run native firmware and portal regression checks after dependency installation."""
from pathlib import Path
import os
import subprocess
root = Path(__file__).resolve().parents[2]
firmware = root / 'firmware'
headers = firmware / '.pio/libdeps/sdpro-weather/ArduinoJson/src'
if not headers.exists():
    headers = firmware / '.pio/libdeps/sdpro/ArduinoJson/src'
if not headers.exists() and os.environ.get('ARDUINOJSON_INCLUDE'):
    headers = Path(os.environ['ARDUINOJSON_INCLUDE'])
if not headers.exists():
    raise SystemExit('Build sdpro-weather first to install ArduinoJson headers')
out = root / '.cache/tests'
out.mkdir(parents=True, exist_ok=True)
# LeakSanitizer cannot enumerate processes in the managed execution sandbox.
env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
for name in ['ticker', 'wifi_retry', 'time_format', 'forecast_labels', 'air_quality', 'api_refresh', 'display_settings', 'city_settings', 'stored_config']:
    binary = out / name
    sources = [str(firmware / f'tests/{name}_test.cpp')]
    if name in ['stored_config', 'display_settings', 'city_settings']:
        sources.append(str(firmware / 'src/StoredConfig.cpp'))
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', '-g',
                    '-I', str(firmware / 'src'), '-I', str(headers), *sources, '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True, env=env)
    print(f'PASS: {name}', flush=True)
for script in ['city_portal_test.cjs', 'ticker_portal_test.cjs']:
    subprocess.run(['node', f'tests/{script}'], cwd=firmware, check=True)
for script in ['display_layout_test.py', 'ticker_layout_test.py', 'api_urls_test.py', 'export_firmware_test.py']:
    subprocess.run(['python3', str(firmware / 'tests' / script)], cwd=root, check=True)
