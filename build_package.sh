#!/usr/bin/env bash
# Build this application and its helper tools; package through one implementation.
set -euo pipefail
cd "$(dirname "$0")"
export PATH="/opt/homebrew/bin:$PATH"
if [[ "$(uname -s)" == Darwin ]]; then
  export CONFIG_TOOLCHAIN_PATH="${CONFIG_TOOLCHAIN_PATH:-/opt/homebrew/bin}"
fi
if [[ "${1:-}" == --skip-build ]]; then
  shift
else
  CardputerZero=y scons -j"${BUILD_JOBS:-4}"
fi
python3 tools/package_deb.py "$@"
