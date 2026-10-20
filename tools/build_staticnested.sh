#!/usr/bin/env bash
set -euo pipefail
RFID_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RFID_CC="${CC:-aarch64-linux-gnu-gcc}"
if ! command -v "$RFID_CC" >/dev/null 2>&1; then RFID_CC=/opt/homebrew/bin/aarch64-linux-gnu-gcc; fi
RFID_SRC="$RFID_ROOT/main/tools/mfkey"
mkdir -p "$RFID_ROOT/dist_mfkey"
"$RFID_CC" -O2 -static -pthread -I"$RFID_SRC" \
  "$RFID_SRC/staticnested.c" "$RFID_SRC/nested_util.c" \
  "$RFID_SRC/crapto1/crapto1.c" "$RFID_SRC/crapto1/crypto1.c" \
  "$RFID_SRC/crapto1/bucketsort.c" "$RFID_SRC/util_posix.c" \
  -lm -o "$RFID_ROOT/dist_mfkey/staticnested"
