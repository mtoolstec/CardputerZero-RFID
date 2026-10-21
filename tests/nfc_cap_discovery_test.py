"""Hardware discovery check; start NFC CAP MFKey32 with the expected UID first.

Uses the user's pn532-python reader library. Does not write a tag or perform
authentication; success validates NFC-A discovery, not MFKey32 key recovery.
"""
import argparse
import json
import sys
import time
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pn532-repo", type=Path, required=True)
    parser.add_argument("--port", required=True)
    parser.add_argument("--uid", default="11223344")
    parser.add_argument("--attempts", type=int, default=10)
    args = parser.parse_args()
    if args.attempts < 1:
        parser.error("--attempts must be positive")
    sys.path.insert(0, str(args.pn532_repo / "script"))
    import pn532_com
    from pn532_cmd import Pn532CMD, Command

    device = pn532_com.Pn532Com()
    passed = 0
    try:
        device.open(args.port)
        command = Pn532CMD(device)
        print("Firmware:", command.get_firmware_version(), flush=True)
        for attempt in range(1, args.attempts + 1):
            command._restart_iso14443a_field()
            tags = command.hf_14a_scan() or []
            records = [
                {k: v.hex().upper() if isinstance(v, (bytes, bytearray)) else v
                 for k, v in tag.items()}
                for tag in tags
            ]
            ok = any(tag.get("uid") == args.uid.upper()
                     and tag.get("atqa") == "0004"
                     and tag.get("sak") == "08" for tag in records)
            passed += ok
            print(json.dumps({"scan": attempt, "ok": ok, "tags": records}), flush=True)
            device.send_cmd_sync(Command.RFConfiguration, b"\x01\x00", timeout=0.5)
            time.sleep(0.3)
    finally:
        if device.isOpen():
            try:
                device.send_cmd_sync(Command.RFConfiguration, b"\x01\x00", timeout=0.5)
            finally:
                device.close()
    print(f"PASS {passed}/{args.attempts}", flush=True)
    return 0 if passed == args.attempts else 1


if __name__ == "__main__":
    sys.exit(main())
