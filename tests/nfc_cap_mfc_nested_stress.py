"""Repeated PN532 nested A/B authentication against the original CAP JSON."""
import json
import os
from pathlib import Path
from nfc_cap_mfc_json_readback import SOURCE, run_case
from nfc_cap_mfkey_matrix_test import automation, remote

RESULT = Path(__file__).parent / 'results/nfc-cap-mfc-nested-stress-2026-10-21.json'


def main():
    if not os.environ.get('SSHPASS'):raise SystemExit('Set SSHPASS')
    original=remote('cat '+SOURCE)
    document=json.loads(original)
    report={'cases':[], 'build':remote('sha256sum /usr/share/APPLaunch/apps/rfid/M5CardputerZero-RFID').split()[0]}
    try:
        for cycle in range(3):
            for key_b in (False,True):
                case=run_case(document,key_b,range(16),nested=True)
                case['cycle']=cycle+1
                report['cases'].append(case)
                print('cycle',cycle+1,'key',case['key_type'],'passed',flush=True)
        report['passed']=True
    except Exception as exc:
        report['passed']=False;report['error']=repr(exc)
        raise
    finally:
        report['source_unchanged']=remote('cat '+SOURCE)==original
        RESULT.write_text(json.dumps(report,indent=2)+'\n')
        automation('SPI_RUN MFC')
        print('passed',report['passed'],flush=True)

if __name__=='__main__': main()
