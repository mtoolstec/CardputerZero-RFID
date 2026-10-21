MFC Saved write validation

NFC CAP hardware discovery can be checked from the host with a PN532 placed
beside its antenna. Start MFKey32 → 2 Log with UID `11223344`, then run:

```
python3 tests/nfc_cap_discovery_test.py \
  --pn532-repo /Users/wilson/Github/pn532-python \
  --port /dev/cu.usbserial-14430 --attempts 10
```

Each attempt cycles the reader RF field and requires UID `11223344`, ATQA
`0004`, SAK `08`. The reader field is turned off on exit. This checks discovery
only; it does not validate encrypted authentication or MFKey32 recovery.
The CAP reset sequence includes the Set Default/overheat-protection erratum
used by the board reference driver (`FC 04 10` immediately after `C1`):
https://github.com/CardputerZero/Cap-CC1101-NFC/blob/main/src/nfc/st25r3916_driver.cpp

Cross-compile the fake ST25R backend test and run it on the Linux device:

```
aarch64-linux-gnu-g++ -std=c++17 -O2 -pthread -Imain \
  -I../CardputerZero-Emulator/vendor tests/mfc_saved_write_test.cpp \
  main/hal/hal_paths_rfid.c -o /tmp/mfc-saved-write-test
```

The test covers Gen1A/Gen4 full dumps, malformed/incomplete blocks, invalid
access-bit complements, capacity mismatch, rejected writes and read-back mismatch.
It does not replace hardware tests of encrypted MFC parity/authentication.

On M5 NFC CAP: connect on Read, open Saved, select an MFC record, Enter →
Write to Tag. Choose Gen1A, Normal / Gen2, Gen3 or Gen4 with F/X (no Fn).
Normal/Gen2 writes data blocks only (47 for 1K), preserving manufacturer block,
keys and access bits. Gen3 adds the proprietary manufacturer-block write; it
also preserves sector trailers. Gen1A/Gen4 write all blocks, trailers last and
block 0 last. Gen1A reuses one backdoor session, checks both WRITE ACKs per block,
and finishes without read-back verification. Gen4 retains a separate verification
pass after all writes. Full UID writes currently require 4-byte UID cards and matching
Mini/1K/4K capacity. Selecting Gen4 opens a password dialog prefilled with
00000000; Enter confirms the password and starts writing.

Except for Gen1A, writes complete in one pass followed by a separate read-back
verification pass. These modes require every written block to match. Failure reports its phase and block
index; there is no rollback of blocks already written.
Proprietary command values were cross-checked against Proxmark3:
https://github.com/RfidResearchGroup/proxmark3/blob/master/client/src/mifare/gen4.h
https://github.com/RfidResearchGroup/proxmark3/blob/master/armsrc/mifarecmd.c

App automation (device-local /tmp/applaunch_nfc_automation.cmd): SAVE,
WRITE_SAVED <saved UID> <mode 0..4>, WRITE_RESULT. These commands write physical
cards; use a backed-up, authorized test card. READ_RESULT provides a full dump
for an independent post-write byte comparison.

For NFC CAP, the app loads the device's existing
`spi0-spidev2-gpio22-overlay.dtbo` at startup if needed and prefers
`/dev/spidev0.2` (kernel GPIO22 CS). The overlay is runtime-only; no boot
configuration is edited. A missing/failed overlay leaves other transports
available. Software CS through `/dev/spidev0.1` is not a validated listener path.

NFC CAP MFKey32 capture was validated with the host PN532 authentication script:

```
python3 /Users/wilson/Github/pn532-python/script/test_usb_mf1_auth.py \
  --sector 0 --key FFFFFFFFFFFF --uid 11223344 --repeat 4
python3 /Users/wilson/Github/pn532-python/script/test_usb_mf1_auth.py \
  --sector 5 --key-type B --key 123456789ABC --uid 11223344 --repeat 4
```

Start a fresh `MFKEY_START 11223344` capture before each group. Run
`MFKEY_CALCULATE`, then query `MFKEY_RESULTS` until calculation completes.
Both groups captured four complete NT/NR/AR exchanges and recovered the exact
supplied keys on 2026-10-21. PN532 AUTH status `14` is expected: this listener
collects authentication attempts and does not implement successful encrypted
MFC access. Logs are in `tests/results/nfc-cap-mfkey-*-2026-10-21.log`.

The SPI listener disables CRC/parity processing for encrypted NR/AR, receives
72 bits, removes the eight encrypted parity bits, and preserves RX events
coalesced with TX completion. File IO for raw SPI transfers is suppressed while
listening; protocol frame/capture logs remain available as `MFKEY-SPI`.
