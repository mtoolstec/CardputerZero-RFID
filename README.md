# CardputerZero RFID

RFID standalone app for CardputerZero — read, write, dump, emulate, sniff and crack
NFC tags across multiple hardware devices.

---

## Hardware and release scope

- **M5 NFC CAP (SPI):** Gen1A dump and Saved writes; Gen1A checks command/data ACKs and skips read-back verification. NTAG JSON emulation and MFKey32 capture are supported. Encrypted MFC 1K emulation is experimental and disabled by default.
- **M5 NFC Unit / GroveNFC (IIC):** reader, dump, writer and listener support depends on the detected chip and card protocol.
- **PN532 / PN532Killer (USB/UART):** authenticated reads and protocol tools; Killer adds firmware-backed slots and capture capabilities.
- **UHF reader (USB):** inventory and bank capture with explicit partial/error status.

See [FEATURE_MATRIX.md](FEATURE_MATRIX.md) for per-device details. A visible UID does not establish successful encrypted MFC authentication. Experimental SPI MFC support is not part of the stable release claim.

## UI — 4-Tab Workflow

### Read Tab
- **Scan tags** — detect ISO14443A (MFC/NTAG/MFU), ISO15693, UHF in field
- **Dump** — read all memory blocks (sector-level for MFC with key auth)
- **Save** — persist scanned dump to local storage
- **Hex log** — Ctrl+L toggles full TX/RX frame log overlay
- **Auto-detect** card type and display ATQA/SAK/UID
- **UHF inventory** — F/X selects a tag and pauses inventory; Ctrl+S immediately
  adds it to Saved, then reads its memory banks in the background. Each bank
  records its hex data, completeness, and error/status. Protected or truncated
  banks are explicitly marked partial.

### Saved Tab
- Browse saved dumps, rename, edit raw hex
- Upload dump to emulator slot (PN532Killer HW slots 0-7)
- Export / delete records
- UHF records show a read-only bank viewer (Tab changes bank, Up/Down scrolls)

### Emulator Tab
- **PN532Killer HW Emulation** — up to 8 slots, ISO14443A / ISO15693 / MIFARE Classic
- **GroveNFC Tag Emulation** — NTAG213/215/216, MFC1K, China2, ISO15693 via I²C register protocol
- **NFC Unit (ST25R3916B) Emulation** — NFC-A / NTAG listener emulation (ISO15693 tag emulation requires GroveNFC)
- **M5 NFC CAP (ST25R3916) Emulation** — ISO14443A listener (REQA → anticollision → UID response)
- **NDEF URL Emulation** — PN532-only: broadcast a custom NDEF URI/URL
- **UID Changer** — rewrite block 0 of Gen1A / Gen2 (CUID) / Gen3 / Gen4 magic cards
- **Dump from emulator** — read back emulated slot content as hex lines
- **Profile switcher** — toggle protocol/slot and auto-reprobe

### Tools Tab
| Tool | Description |
|------|-------------|
| **MIFARE Keys** | Manage key dictionary: add/edit/disable default keys, import `.dic` files, load `.mfd` keys from dumps |
| **UID Changer** | Change UID on magic cards: detect Gen1A/Gen2(CUID)/Gen3/Gen4, write block 0 with custom UID |
| **MFKey32v2** | Sniff MIFARE Classic authentication + crack keys via PN532Killer (no tag required) |
| **MFKey64** | Sniff MIFARE Classic authentication + crack keys via PN532Killer (tag required for non-UID) |
| **Staticnested** | PN532Killer: authenticate sectors A/B using built-in defaults plus enabled MIFARE Keys, then recover unknown static-nonce keys from a known seed and verify candidates |

## Attack / Cracking Methods

| Method | Description | Supported On |
|--------|-------------|--------------|
| **Default Keys** | Try known default keys from key dictionary to authenticate sectors | PN532, PN532Killer, NFC Unit, GroveNFC, NFC CAP |
| **Basic Auth Read** | Least-privilege: try keys only from selected dumps | PN532, PN532Killer |
| **Darkside** | MFC authentication attack (for unknown keys) | PN532Killer |
| **Hardnested** | Brute-force nested authentication attack | PN532Killer |
| **MFKey32v2** | Offline key recovery from sniffed nonces | PN532Killer + crapto1 |
| **MFKey64** | Offline key recovery from sniffed nonces (tag present) | PN532Killer + crapto1 |
| **UHF Continuous Scan** | Poll UHF tags at interval, table view with RSSI/count/antenna | UHF Reader |

---

## Data Storage

| Path | Contents |
|------|----------|
| `~/rfid/dumps/` | Saved tag dumps (JSON, raw hex blocks) |
| `~/rfid/mifare_keys.json` | MIFARE key dictionary (A/B keys per sector) |
| `~/rfid/emulator_config.json` | Emulator slot config (data per slot) |
| `~/rfid/uart_config.json` | UART port settings (device path, baud) |
| `~/rfid/last_transport.json` | Last connected endpoint (reconnect on restart) |
| `~/rfid/nfc_data/logs/` | Daily hex TX/RX log files |
| Override via env: `M5CZ_NFC_ROOT_DIR`, `M5CZ_NFC_RECORDS_DIR` | Custom storage paths |

---

## Project Layout

| Directory | Purpose |
|---|---|
| `main/src/` | Application entry, LVGL and keyboard setup |
| `main/nfc/` | Device drivers, transport, protocol, service and storage |
| `main/ui/` | LVGL screens and input handling |
| `main/hal/` | Board paths, settings, screenshot and panel recovery |
| `main/tools/mfkey/` | MFKey32v2, MFKey64, Staticnested and Crypto1 sources |
| `tests/` | Protocol regression tests and optional hardware test scripts |
| `tools/` | Packaging, regression runner and development utilities |
| `tools/rfid_mfc_tx/` | Experimental kernel helper, excluded from the package |
| `config/` | Bundled emulator defaults and key dictionary |
| `applications/`, `share/`, `screenshots/` | Launcher and store assets |

Local captures in `tests/results/` and generated build output are excluded from Git.
Panel recovery reads the inversion mode from the board firmware and restores it
at startup, after CAP operations, and on clean exit. The original intermittent
mode change remains under investigation; recovery is not evidence of its cause.

## Build

```bash
cd projects/RFID
CardputerZero=y scons -j4
```

Output:

- `dist/M5CardputerZero-RFID` — main app binary
- `dist_mfkey/mfkey32v2`, `dist_mfkey/mfkey64`, `dist_mfkey/staticnested` — helper tools

## Package as .deb

```bash
cd projects/RFID
python3 tools/package_deb.py --maintainer "yourname <you@example.com>"
```

Output: `build/rfid_1.0.3-1_arm64.deb`

Install:

```bash
scp build/rfid_1.0.3-1_arm64.deb pi@<device-ip>:/tmp/
ssh pi@<device-ip> "sudo dpkg -i /tmp/rfid_1.0.3-1_arm64.deb"
```

## APPLaunch Integration

- Desktop entry: `applications/rfid.desktop`
- Binary: `/usr/share/APPLaunch/apps/rfid/M5CardputerZero-RFID`
- Helper tools: `/usr/share/APPLaunch/apps/rfid/bin/`

## Notes

- NFC storage and key files are persisted under `~/rfid/` (or `hal_path_data_dir()`).
- For deployment from APPLaunch, package the binary and helpers with `./build_package.sh --skip-build`.
- Hex logging auto-rotates daily under `~/rfid/nfc_data/logs/`.

## Regression and release

```bash
SSHPASS='<device password>' python3 tools/run_tests.py --device pi@192.168.0.120
./build_package.sh
```

The regression runner uses fake transports and temporary files, without tag writes.
Physical antenna tests are separate; see [tests/README.md](tests/README.md).
Store submission records and remaining limitations are in [PUBLISH.md](PUBLISH.md).
