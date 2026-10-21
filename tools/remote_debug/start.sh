#!/usr/bin/env bash
set -euo pipefail
DEVICE="${1:-pi@192.168.0.120}"
PORT="${PORT:-18765}"
SOURCE_DIR="$(cd "$(dirname "$0")" && pwd)"
CONTROL_DIR="$(mktemp -d /tmp/cardputer-debug.XXXXXX)"
CONTROL="$CONTROL_DIR/ssh"
cleanup() { ssh -S "$CONTROL" -O exit "$DEVICE" 2>/dev/null || true; rm -rf "$CONTROL_DIR"; }
trap cleanup EXIT INT TERM
# Authentication happens once; no password is stored in this project.
ssh -M -S "$CONTROL" -o ControlPersist=60 -fN "$DEVICE"
scp -o "ControlPath=$CONTROL" "$SOURCE_DIR/server.py" "$DEVICE:/tmp/cardputer-debug-server.py"
ssh -S "$CONTROL" "$DEVICE" "sudo -n sh -c 'if ! pgrep -f \"^python3 /tmp/cardputer-debug-server.py\" >/dev/null; then nohup python3 /tmp/cardputer-debug-server.py >/tmp/cardputer-debug-server.log 2>&1 </dev/null & fi'"
URL="http://127.0.0.1:$PORT"
echo "屏幕及键盘调试：$URL（Ctrl+C 关闭隧道）"
(sleep 2; if command -v open >/dev/null; then open "$URL"; elif command -v xdg-open >/dev/null; then xdg-open "$URL"; fi) &
ssh -S "$CONTROL" -o ExitOnForwardFailure=yes -o ServerAliveInterval=15 -N -L "127.0.0.1:$PORT:127.0.0.1:18765" "$DEVICE"
