#!/usr/bin/env python3
"""Merge the current ESP-IDF build into one image and verify every input range."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile


def main():
    project = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=project / 'build')
    args = parser.parse_args()
    build = args.build_dir.resolve()
    description = json.loads((build / 'project_description.json').read_text())
    flash = json.loads((build / 'flasher_args.json').read_text())
    settings = flash['flash_settings']
    if description['target'] != 'esp32p4' or settings['flash_size'] != '16MB':
        raise ValueError('This hardware requires an ESP32-P4 build with 16 MB flash')
    if Path(description['project_path']).resolve() != project:
        raise ValueError('Build belongs to another project; rebuild this checkout first')
    version = description['project_version']
    if not re.fullmatch(r'[A-Za-z0-9._-]+', version):
        raise ValueError('Invalid firmware version')
    images = sorted((int(address, 0), (build / filename).resolve())
                    for address, filename in flash['flash_files'].items())
    previous_end = 0
    for address, path in images:
        if not path.is_relative_to(build) or not path.is_file():
            raise ValueError(f'Invalid build image path: {path}')
        if address < previous_end:
            raise ValueError('Flash image ranges overlap')
        previous_end = address + path.stat().st_size
    if not images or previous_end > 16 * 1024 * 1024:
        raise ValueError('Merged image does not fit the flash')

    output_dir = project / 'firmware'
    output_dir.mkdir(exist_ok=True)
    output = output_dir / f"{description['project_name']}_v{version}_merged.bin"
    with tempfile.NamedTemporaryFile(dir=output_dir, prefix='.merge-', suffix='.bin', delete=False) as file:
        temporary = Path(file.name)
    try:
        command = [sys.executable, '-m', 'esptool', '--chip', 'esp32p4', 'merge-bin',
                   '--output', str(temporary), '--format', 'raw', '--target-offset', '0x0',
                   '--flash-mode', settings['flash_mode'], '--flash-freq', settings['flash_freq'],
                   '--flash-size', settings['flash_size']]
        for address, path in images:
            command.extend((hex(address), str(path)))
        subprocess.run(command, check=True)
        merged = temporary.read_bytes()
        if len(merged) != previous_end:
            raise ValueError('Unexpected merged image size')
        previous_end = 0
        for address, path in images:
            content = path.read_bytes()
            if merged[address:address + len(content)] != content:
                raise ValueError(f'Merged data differs from build image: {path.name}')
            if any(value != 0xFF for value in merged[previous_end:address]):
                raise ValueError('Unexpected data between flash images')
            previous_end = address + len(content)
        temporary.chmod(0o644)
        temporary.replace(output)
    finally:
        temporary.unlink(missing_ok=True)
    checksum = hashlib.sha256(output.read_bytes()).hexdigest()
    (output_dir / 'SHA256SUMS').write_text(f'{checksum}  {output.name}\n', encoding='ascii')
    print(f'Verified merged firmware: {output}')
    print(f'Flash address: 0x0; size: {output.stat().st_size} bytes; SHA-256: {checksum}')


if __name__ == '__main__':
    main()
