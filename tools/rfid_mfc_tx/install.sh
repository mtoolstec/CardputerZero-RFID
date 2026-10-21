#!/bin/sh
# Build on the Cardputer Zero with matching installed kernel headers.
set -eu
if [ "$(id -u)" -ne 0 ]; then exec sudo -n "$0" "$@"; fi
driver_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
kernel_release=$(uname -r)
make -C "/lib/modules/$kernel_release/build" M="$driver_dir" modules -j2
if [ -d /sys/module/rfid_mfc_tx ]; then rmmod rfid_mfc_tx; fi
install -D -m644 "$driver_dir/rfid_mfc_tx.ko" "/lib/modules/$kernel_release/extra/rfid_mfc_tx.ko"
depmod -a "$kernel_release"
install -m644 "$driver_dir/70-rfid-mfc-tx.rules" /etc/udev/rules.d/70-rfid-mfc-tx.rules
udevadm control --reload-rules
modprobe rfid_mfc_tx
udevadm trigger --action=add --subsystem-match=misc --sysname-match=rfid_mfc_tx
udevadm settle --timeout=10
ls -l /dev/rfid_mfc_tx
