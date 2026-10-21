#!/usr/bin/env python3
"""Cross-compile protocol tests and run them on an authorized Linux device.

These use fake transports or temporary files; they do not write physical tags.
Set SSHPASS to use password authentication, or use an existing SSH identity.
"""
import argparse
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
TESTS = ('panel_mode_test', 'mfc_key_export_test', 'mfc_saved_write_test',
         'mfc_emulator_protocol_test', 'test_mifare_standard', 'test_pn532_sniffer')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--device', required=True, help='SSH destination, e.g. pi@192.168.0.120')
    parser.add_argument('--compiler', default='aarch64-linux-gnu-g++')
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if not compiler:
        parser.error(f'Compiler not found: {args.compiler}')
    control = '/tmp/rfid-tests-ssh-' + str(os.getpid())
    ssh = ['ssh', '-o', 'ConnectTimeout=5', '-o', 'ControlMaster=auto',
           '-o', 'ControlPersist=60', '-o', 'ControlPath=' + control, args.device]
    login = ssh
    if os.environ.get('SSHPASS'):
        login = ['sshpass', '-e'] + ssh[:1] + ['-o', 'PubkeyAuthentication=no'] + ssh[1:]
    subprocess.run(login + ['true'], stdin=subprocess.DEVNULL, check=True, timeout=15)
    output = ROOT / 'build' / 'tests'
    output.mkdir(parents=True, exist_ok=True)
    for name in TESTS:
        binary = output / name
        subprocess.run([compiler, '-std=c++17', '-O2', '-pthread', '-Imain', '-Imain/nfc',
                        '-I../CardputerZero-Emulator/vendor', f'tests/{name}.cpp',
                        'main/hal/hal_paths_rfid.c', '-o', str(binary)], cwd=ROOT, check=True)
        remote = '/tmp/rfid-test-' + name
        with binary.open('rb') as data:
            subprocess.run(ssh + ['cat > ' + shlex.quote(remote)], stdin=data, check=True, timeout=30)
        subprocess.run(ssh + ['chmod +x ' + shlex.quote(remote) + ' && ' + shlex.quote(remote)],
                       check=True, timeout=120)
    print(f'PASS: {len(TESTS)} test suites')


if __name__ == '__main__':
    main()
