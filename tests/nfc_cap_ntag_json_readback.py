"""Real PN532 readback of CAP JSON, including reload; restores the source JSON."""
import json
import os
from pathlib import Path
import shlex
import sys
import time
from nfc_cap_mfkey_matrix_test import automation, remote

sys.path.insert(0, '/Users/wilson/Github/pn532-python/script')
from pn532_com import Pn532Com
from pn532_cmd import Pn532CMD
from pn532_enum import Command

SOURCE = '/home/pi/rfid/emulator/NTAG/ntag213_0.json'
RESULT = Path(__file__).parent / 'results/nfc-cap-ntag-json-readback-2026-10-21.json'


def readback(expected, profile):
    start = automation('SPI_RUN ' + profile)
    p = Pn532Com()
    p.open('/dev/cu.usbserial-14430')
    cmd = Pn532CMD(p)
    try:
        cmd._restart_iso14443a_field()
        tags = cmd.hf_14a_scan()
        assert len(tags) == 1, tags
        uid = bytes(expected[:3] + expected[4:8])
        assert bytes(tags[0]['uid']) == uid, tags
        chunks = []
        for page in range(0, len(expected)//4, 4):
            r = p.send_cmd_sync(Command.InCommunicateThru, bytes([0x30, page]), timeout=1)
            assert r.status == 0 and len(r.data) == 16, (page, r.status, r.data.hex())
            count = min(16, len(expected) - page*4)
            wanted = bytes(expected[(page*4+i) % len(expected)] for i in range(16))
            assert bytes(r.data) == wanted, (page, r.data.hex())
            chunks.append({'page': page, 'data': r.data.hex()})
        version = p.send_cmd_sync(Command.InCommunicateThru, b'\x60', timeout=1)
        assert version.status == 0 and len(version.data) == 8, version.data.hex()
        assert version.data[6] == (0x0f if profile == 'NTAG213' else 0x11), version.data.hex()
        return dict(start=start, uid=uid.hex(), version=version.data.hex(), verified_bytes=len(expected), reads=chunks, passed=True)
    finally:
        p.send_cmd_sync(Command.RFConfiguration, b'\x01\x00', timeout=.5)
        p.close()


def main():
    if not os.environ.get('SSHPASS'):
        raise SystemExit('Set SSHPASS for the authorized device login')
    original = remote('cat ' + SOURCE)
    report = {'cases': []}
    try:
        document = json.loads(original)
        memory = bytes.fromhex(''.join(v for _, v in sorted(document['blocks'].items(), key=lambda kv:int(kv[0]))))
        report['cases'].append(readback(memory, 'NTAG213'))
        document['blocks']['04'] = 'DEADBEEF'
        changed = json.dumps(document)
        remote('printf %s ' + shlex.quote(changed) + ' > ' + SOURCE)
        replacement = memory[:16] + bytes.fromhex('DEADBEEF') + memory[20:]
        report['cases'].append(readback(replacement, 'NTAG213'))
        # The larger profile pads a smaller dump and advertises its actual capacity.
        report['cases'].append(readback(replacement.ljust(540, b'\x00'), 'NTAG215'))
        report['passed'] = True
    except Exception as exc:
        report['passed'] = False
        report['error'] = repr(exc)
        raise
    finally:
        remote('printf %s ' + shlex.quote(original) + ' > ' + SOURCE)
        report['source_restored'] = remote('cat ' + SOURCE) == original
        RESULT.parent.mkdir(parents=True, exist_ok=True)
        RESULT.write_text(json.dumps(report, indent=2) + '\n')
        automation('SPI_RUN NTAG213')
        print(json.dumps({k:v for k,v in report.items() if k != 'cases'}))

if __name__ == '__main__':
    main()
