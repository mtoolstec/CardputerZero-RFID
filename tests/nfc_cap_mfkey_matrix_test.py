"""On-device MFKey32 regression matrix; requires the user's PN532 test setup.

Set SSHPASS for the existing device login. No tag writes or key imports occur.
PN532 authentication is expected to fail: success means captures recover the
exact supplied test key, not that the listener implements authenticated reads.
"""
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parent
OUTPUT = ROOT / 'results' / 'nfc-cap-mfkey-matrix-2026-10-21.json'
CASES = [
    ('11223344', 0, 'A', 'FFFFFFFFFFFF'),
    ('11223344', 1, 'B', '000000000000'),
    ('A1B2C3D4', 5, 'A', 'A0A1A2A3A4A5'),
    ('A1B2C3D4', 15, 'B', 'D3F7D3F7D3F7'),
    ('01020304', 16, 'A', '123456789ABC'),
    ('01020304', 31, 'B', 'FFFFFFFF0000'),
    ('DEADBEEF', 32, 'A', '0123456789AB'),
    ('DEADBEEF', 39, 'B', 'C0FFEE123456'),
]
SSH = ['sshpass', '-e', 'ssh', '-o', 'PubkeyAuthentication=no',
       '-o', 'ConnectTimeout=5', '-o', 'ControlMaster=auto',
       '-o', 'ControlPath=/tmp/rfid-mfkey-matrix-ssh', '-o', 'ControlPersist=120',
       'pi@192.168.0.120']
STATUS = '/tmp/applaunch_nfc_automation.status'
CMD = '/tmp/applaunch_nfc_automation.cmd'


def remote(command):
    for attempt in range(3):
        try:
            return subprocess.check_output(SSH + [command], text=True, timeout=15).strip()
        except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
            if attempt == 2:
                raise
            time.sleep(0.5)


def automation(command):
    remote(f': > {STATUS}; printf "%s\\n" {shlex.quote(command)} > {CMD}')
    deadline = time.monotonic() + 12
    while time.monotonic() < deadline:
        result = remote(f'cat {STATUS}')
        if result:
            if not result.startswith('OK '):
                raise RuntimeError(f'{command}: {result}')
            return result
        time.sleep(0.15)
    raise TimeoutError(command)


def main():
    if not os.environ.get('SSHPASS'):
        raise SystemExit('Set SSHPASS for the authorized device login')
    previous = json.loads(OUTPUT.read_text()) if '--retry-failed' in sys.argv and OUTPUT.exists() else None
    report = {'cases': [], 'note': 'PN532 status 14 expected for capture-only listener; no tag writes.'}
    for uid, sector, key_type, key in CASES:
        old = next((c for c in previous['cases'] if c['uid'] == uid and c['sector'] == sector and c['key_type'] == key_type and c.get('passed')), None) if previous else None
        if old:
            report['cases'].append(old)
            continue
        case = dict(uid=uid, sector=sector, key_type=key_type, expected_key=key)
        started = time.monotonic()
        try:
            case['start'] = automation('MFKEY_START ' + uid)
            process = subprocess.run([
                sys.executable, '/Users/wilson/Github/pn532-python/script/test_usb_mf1_auth.py',
                '--port', '/dev/cu.usbserial-14430', '--sector', str(sector),
                '--key-type', key_type, '--key', key, '--uid', uid, '--repeat', '4',
            ], capture_output=True, text=True, timeout=40)
            case['pn532_output'] = process.stdout + process.stderr
            frames = [json.loads(line) for line in process.stdout.splitlines() if line.startswith('{')]
            assert len(frames) == 4 and all(f.get('uid') == uid and f.get('response') == '14' for f in frames), case['pn532_output']
            case['capture'] = automation('MFKEY_CAPTURE_STATUS')
            assert 'logs=4 ' in case['capture'] and 'ready=1' in case['capture'], case['capture']
            automation('MFKEY_CALCULATE')
            deadline = time.monotonic() + 45
            while True:
                result = automation('MFKEY_RESULTS')
                if result != 'OK calculation running':
                    break
                if time.monotonic() >= deadline:
                    raise TimeoutError('MFKey computation')
                time.sleep(0.3)
            case['result'] = result
            expected = f'{int(uid, 16):x} sector={sector} key={key}'
            assert expected.lower() in result.lower(), result
            case['passed'] = True
        except Exception as exc:
            case['passed'] = False
            case['error'] = str(exc)
        case['elapsed_s'] = round(time.monotonic() - started, 2)
        report['cases'].append(case)
        OUTPUT.write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps(case), flush=True)
    report['passed'] = sum(c['passed'] for c in report['cases'])
    report['total'] = len(CASES)
    OUTPUT.write_text(json.dumps(report, indent=2) + '\n')
    print(f"PASS {report['passed']}/{report['total']}", flush=True)
    automation('MFKEY_START 11223344')
    return 0 if report['passed'] == report['total'] else 1


if __name__ == '__main__':
    sys.exit(main())
