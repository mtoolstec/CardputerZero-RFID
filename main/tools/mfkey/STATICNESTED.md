# Staticnested helper

`staticnested.c` and `nested_util.c/.h` are copied from the local
`pn532-python/third_party/mifare/tools/mfc` implementation. They share the
existing Proxmark3 Crypto1 code in this directory. Preserve upstream notices;
the Crypto1/common dependency notices specify GPL-3.0-or-later.

Build: `bash tools/build_staticnested.sh` from the RFID project.
The application passes explicit `0x` prefixes (decimal-only looking hex values
must not be parsed as decimal), parses each `Key N...` candidate, and verifies
it by authenticating the current card before presenting it as recovered.

The protocol accepts both the reference 21-byte session and the 22-byte
status-prefixed session observed on the device. Nonzero status is rejected.
