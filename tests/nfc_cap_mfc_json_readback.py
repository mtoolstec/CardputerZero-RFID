"""PN532 integration: real CAP Crypto1 auth/read, JSON reload, keys and recovery.

Run with SSHPASS and the authorized device/PN532 connected. Source is restored
in finally. Checks fresh RF sessions and nested authentication within one RF session.
"""
import json
import os
from pathlib import Path
import shlex
import sys
from nfc_cap_mfkey_matrix_test import automation, remote

sys.path.insert(0, '/Users/wilson/Github/pn532-python/script')
from pn532_com import Pn532Com
from pn532_cmd import Pn532CMD
from pn532_enum import Command

SOURCE = '/home/pi/rfid/emulator/MFC/mfc1k_0.json'
RESULT = Path(__file__).parent / 'results/nfc-cap-mfc-json-readback-2026-10-21.json'


def run_case(document, key_b, sectors, nested=False):
    memory = bytes.fromhex(''.join(v for _, v in sorted(document['blocks'].items(), key=lambda kv:int(kv[0]))))
    start = automation('SPI_RUN MFC')
    p = Pn532Com()
    p.open('/dev/cu.usbserial-14430')
    cmd = Pn532CMD(p)
    result = dict(start=start, uid=memory[:4].hex(), key_type='B' if key_b else 'A', nested=nested, sectors=[])
    try:
        for index, sector in enumerate(sectors):
            if not nested or index==0:
                cmd._restart_iso14443a_field()
                tags = cmd.hf_14a_scan()
                assert tags and bytes(tags[0]['uid']) == memory[:4], tags
            trailer = (sector*4+3)*16
            key = memory[trailer+(10 if key_b else 0):trailer+(16 if key_b else 6)]
            auth = p.send_cmd_sync(Command.InDataExchange, bytes([1,0x61 if key_b else 0x60,sector*4])+key+memory[:4], timeout=1)
            assert auth.status == 0, ('auth', sector, auth.status)
            reads=[]
            for block in list(range(sector*4,sector*4+4))*2:
                r = p.send_cmd_sync(Command.InDataExchange, bytes([1,0x30,block]),timeout=1)
                wanted = bytearray(memory[block*16:block*16+16])
                if block%4==3: wanted[:6]=bytes(6)
                assert r.status==0 and bytes(r.data)==wanted, (block,r.status,r.data.hex(),wanted.hex())
                reads.append(dict(block=block,data=r.data.hex()))
            result['sectors'].append(dict(sector=sector,reads=reads))
        # A wrong key must fail and the next fresh RF session must recover.
        cmd._restart_iso14443a_field(); tags=cmd.hf_14a_scan()
        wrong=p.send_cmd_sync(Command.InDataExchange,b'\x01\x60\x00'+bytes.fromhex('010203040506')+memory[:4],timeout=1)
        assert wrong.status != 0, 'wrong key accepted'
        result['wrong_key_status']=wrong.status
        cmd._restart_iso14443a_field(); tags=cmd.hf_14a_scan()
        key=memory[48:54]
        recovered=p.send_cmd_sync(Command.InDataExchange,b'\x01\x60\x00'+key+memory[:4],timeout=1)
        assert recovered.status==0, ('recovery',recovered.status)
        r=p.send_cmd_sync(Command.InDataExchange,b'\x01\x30\x01',timeout=1)
        assert r.status==0 and bytes(r.data)==memory[16:32], ('recovery read',r.status)
        result['recovery']=True
        result['passed']=True
        return result
    finally:
        try:p.send_cmd_sync(Command.RFConfiguration,b'\x01\x00',timeout=.5)
        finally:p.close()


def main():
    if not os.environ.get('SSHPASS'):raise SystemExit('Set SSHPASS')
    original=remote('cat '+SOURCE)
    report={'cases':[]}
    try:
        document=json.loads(original)
        report['cases'].append(run_case(document,False,range(16)))
        # Distinct UID, per-sector keys and block payloads prove JSON is reloaded.
        for block in range(64):
            data=bytearray((block*13+i*7)&255 for i in range(16))
            if block==0:
                data[:5]=bytes.fromhex('DEADBEEF22')
            if block%4==3:
                sector=block//4
                data[:6]=bytes([0xA0+sector])*6
                data[6:10]=bytes.fromhex('FF078069')
                data[10:]=bytes([0xB0+sector])*6
            document['blocks'][f'{block:02d}']=data.hex().upper()
        remote('printf %s '+shlex.quote(json.dumps(document))+' > '+SOURCE)
        report['cases'].append(run_case(document,False,range(16)))
        report['cases'].append(run_case(document,True,range(16)))
        report['cases'].append(run_case(document,False,range(16),nested=True))
        report['cases'].append(run_case(document,True,range(16),nested=True))
        report['passed']=True
    except Exception as exc:
        report['passed']=False;report['error']=repr(exc)
        raise
    finally:
        remote('printf %s '+shlex.quote(original)+' > '+SOURCE)
        report['source_restored']=remote('cat '+SOURCE)==original
        RESULT.write_text(json.dumps(report,indent=2)+'\n')
        automation('SPI_RUN MFC')
        print(json.dumps({k:v for k,v in report.items() if k!='cases'}))

if __name__=='__main__': main()
