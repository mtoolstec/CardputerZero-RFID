# Cardputer Zero MFC transmit helper

The RFID app computes Crypto1 in its RF worker. This helper sends its already
encrypted Manchester waveform and rearms the ST25R3916 receiver before a reader
can send NR/AR after nested NT. Encrypted parity stays correct for all keys,
including FFFFFFFFFFFF. No nonce/parity compatibility workaround is used.

The target is BCM2837 Cardputer Zero: SPI0 at 0x3f204000, GPIO at 0x3f200000,
CAP `/dev/spidev0.2`, SCLK GPIO11 and active-low BSS GPIO22. Other controller
layouts are rejected. The helper uses the existing SPI controller bus lock,
resumes its runtime clock, validates pin state, and sends through the controller
FIFO with interrupts locally disabled. It restores the receiver, pin mux,
controller registers, runtime-power reference and bus lock on completion/error.
MFC frames take up to about 1.6 ms. Input is capped at 512 bytes, with a 5 ms
TX timeout and a separately bounded recovery command. There is no arbitrary
register/address interface. Node permissions match the `spi` group.

Close RFID, copy this directory to the device and run `./install.sh`. Matching
kernel headers and a native C compiler are required. The installer builds the
module for `uname -r`, installs it and registers udev rules. A SPI0.2 add event
loads the module automatically, including the runtime overlay created by RFID
at launch. After a kernel upgrade, rebuild using the new matching headers.
Do not copy a module built for a different kernel version.

The app refuses MFC emulation if the driver cannot be opened. NTAG and MFKey
capture do not require this driver. Emulation currently supports 4-byte UID
MFC 1K authentication/nested authentication and read-only blocks, not WRITE,
value commands or full access-bit enforcement.

Hardware regression (on the Mac with the authorized SSH password in SSHPASS):

```
python3 tests/nfc_cap_mfc_json_readback.py
python3 tests/nfc_cap_mfc_nested_stress.py
```

Validation-only invalid ioctl requests can be checked on the device with
`python3 tests/rfid_mfc_tx_invalid_ioctl.py`; they do not transmit RF data.

RFID 1.0 disables this experimental path by default after a restart regression
failed reader authentication. For development only, set
`NFC_SPI_MFC_EXPERIMENTAL=1` before launching RFID. Installing this module alone
does not enable MFC emulation. The 1.0 store package does not install this module.
