# RFID 1.0 publication record

- Package: `rfid`, version `1.0`, architecture `arm64`
- Debian package: `build/rfid_1.0-1_arm64.deb`
- SHA-256: `87de4c2b31607f835794b34a1f90492ff99aaadd729d6e2b15ba401677455d4d`
- Maintainer: `whywilson <15230131+whywilson@users.noreply.github.com>`
- Source: https://github.com/mtoolstec/CardputerZero-RFID
- Uploader: `whywilson`; email hidden
- Title: RFID; share code: `RFID`; license: MIT
- Categories: Hardware & IoT, Security
- Permissions: additional hardware required; other six store permissions false
- Icon: `share/images/ic_rfid.png` (explicitly uploaded)
- Screenshots: `screenshots/01.png` through `04.png`, four actual 320×170 captures
- Demo: `build/release-1.0/rfid-1.0-review-complete.mp4`, real device framebuffer captured over SSH; includes connection recovery attempts

## Result

- User requested 1.0 publication on 2026-10-21 (Asia/Shanghai).
- Portal: https://dev.cardputer.cc/#/upload
- Accepted processing run: https://github.com/CardputerZero/packages/actions/runs/39574121090
- Review PR: https://github.com/CardputerZero/packages/pull/201
- Demo comment: https://github.com/CardputerZero/packages/pull/201#issuecomment-6312308314
- Final package installed on the device after metadata corrections; executable unchanged from tested 1.0 build.
- State: automated submission validation passed; waiting for first-release maintainer review and merge. Not yet listed as released.
- Initial rejected submission: https://github.com/CardputerZero/packages/issues/200 (template Maintainer and missing explicit store icon; both corrected).

## Validation and limitations

- AArch64 build and Debian install passed; binary SHA-256 `da0756aae6d5e9e66c6a9d9bd17673e48a6f0b7d200eaff4331e214815287938`.
- PN532 NTAG213 original/reloaded JSON and NTAG215 complete readback passed; original source restored.
- Real launcher startup, READ/SAVED/EMU/TOOLS navigation and double-Esc exit checked.
- SPI MFC 1K encrypted emulation FAILED restart authentication regression. Disabled by default in 1.0, including on a device with the helper installed. Explicit development opt-in: `NFC_SPI_MFC_EXPERIMENTAL=1`.
- The 1.0 package does not install the experimental kernel helper. Android MFC compatibility is not claimed.
- CAP external power was found off during one subsequent launch and required restoring the rail during debug. Recorded video includes recovery; repeat-launch power behavior needs further investigation.
- Pre-existing changes remain in the workspace; no blanket source commit was made as part of publishing.

## Local display hotfix (1.0.1, not submitted)

- Removed application-level panel inversion commands from display initialization.
- Package: `build/rfid_1.0.1-1_arm64.deb`; built and installed on the device.
- Hardware state restored to the board firmware setting (DCS 0x21); the attempted 0x20 recovery was incorrect for this panel.
- Physical colour confirmation remains necessary: framebuffer captures do not show panel inversion. The original dark UI palette was not changed.

## Gen1A Key A investigation (2026-10-21)

- User confirmed the launcher physical colours were restored after the factory panel setting was restored.
- Current CAP card UID `4BD0C913`: three complete reads, each `OK 64/64 blocks 1s`; all 16 Key A fields were `FFFFFFFFFFFF`. Report: `tests/results/gen1a-key-a-2026-10-21.json`.
- Historical `A3FC3EFC` zero-Key-A dump identifies its source as PN532Killer/default-key authenticated reading. This does not establish the cause of the user's current CAP report.
- Fixed a separate confirmed key-export defect: CAP decimal block prefixes were included in key bytes. Export now uses the strict block parser and correct 4K trailer spacing. Device-side regression passed.
- Candidate package: `build/rfid_1.0.2-1_arm64.deb`, package `rfid`, version `1.0.2`, architecture `arm64`. Build/package passed. Not installed or submitted; reported CAP zero-Key-A case remains unverified pending the affected card/result.
- Physical-device demo supplied by user: https://youtube.com/shorts/LJY0yvFnH0k
- Posted replacement video link: https://github.com/CardputerZero/packages/pull/201#issuecomment-6314923965
- PR #201 remains open. Do not close it until a replacement submission is ready. A comment alone does not update the Debian artifact.

## Gen1A Saved write speed (2026-10-21)

- Gen1A now reuses the unlocked backdoor session across successful block writes. Failures trigger reselect/unlock before retry; command/data ACKs are still required.
- Removed Gen1A's read-back verification pass at user request. Other modes retain write-then-verify.
- Device fake-backend regression passed: one Gen1A unlock, zero read-back calls, write NAK failures, Gen4 verify and mismatch rejection.
- Installed updated local `rfid` 1.0.2 package. SHA-256: `da26a299dbc923d631d360499ca0253e18eaa027b95dc113bdfa73ab4d06cb3e`.
- Physical CAP/card `4BD0C913`: rewrote its backed-up contents through Saved Gen1A; result `OK 64/64 blocks 2s`. Independent developer read-back matched all 64 original blocks. Report: `tests/results/gen1a-fast-write-2026-10-21.json`.
- Returned to launcher after debug. Store submission remains pending the separate reported zero-Key-A investigation.

## 1.0.3 preparation

- PR #201 is now merged (2026-10-21T11:46:48Z). Preserve it as release history; submit a newer package normally.
- Version: `1.0.3`, package `rfid`, architecture `arm64`.
- Changes: firmware-derived panel-mode recovery at app start, CAP operation boundaries and clean exit; fast Gen1A ACK-only Saved write; correct prefixed dump key export; separate `main/nfc` service/driver directory; remove unused legacy copies and consolidate test/build entry points.
- Six fake-transport/storage/parser regression suites passed on the device via `tools/run_tests.py`. Python, shell, metadata and four screenshot checks passed.
- Recovery executes successfully under the stock `pi` account. Intermittent panel-mode trigger remains unproven; no claim that the SPI root cause has been established.
- Reported CAP zero-Key-A issue was not reproduced on the available card. Do not substitute guessed keys or advertise this separate issue as fixed.
- SPI encrypted MFC 1K emulation remains experimental and disabled by default; kernel helper and development utilities are excluded from the store package.

## 1.0.3 submission result

- Source implementation commit: `d7906b8`, pushed to `origin/main`.
- Package: `build/rfid_1.0.3-1_arm64.deb`; SHA-256 `29663d9f675543fe7b4a2046db3484d13e9d795231ff1bcdcd598906f7371a2e`.
- Installed on device; launched from APPLaunch, clean double-Esc exit and relaunch passed. Stock-user journal confirms firmware mode 0x21 restored at startup, CAP operation completion and exit.
- Current build physical Gen1A Saved write: `OK 64/64 blocks 2s`; independent 64-block comparison against the original backup passed.
- NTAG antenna retest for this build awaits removal of the physical card; earlier successful NTAG tests are historical, not a new result for this build.
- Web submission processing: https://github.com/CardputerZero/packages/actions/runs/39623014032 — success.
- Release PR: https://github.com/CardputerZero/packages/pull/203 — merged `2026-10-21T15:08:57Z`, merge `05cee96aa2e7098951338390044afd5f93e74eb2`.
- Published release descriptor's version, architecture and SHA-256 match the local package. Four existing store screenshots and the explicit icon are retained; uploader email hidden.
- Separate PR validator https://github.com/CardputerZero/packages/actions/runs/39623067326 failed before jobs started, reported by GitHub CLI as a workflow-file issue; the web submission's package validation passed.
- APT index deployment: https://github.com/CardputerZero/packages/actions/runs/39623080956 — success.

## 1.0.4 preparation (2026-11-13)

- Source changes: GroveNFC emulation and read streaming; PN532Killer ISO15693 write readback; NFC Unit listener recovery and MFKey UID input.
- Package: `build/rfid_1.0.4-m5stack1_arm64.deb`; package `rfid`, version `1.0.4`, architecture `arm64`.
- SHA-256: `04f9f9c8a964d3f002e0bd3a1ac190f6648a0744089c95e22d232a11ada1d2d6`.
- Icon: `share/images/ic_rfid.png`; screenshots: `screenshots/01.png` through `04.png`.
- Six fake-transport/parser regression suites passed on the macOS host and the ARM64 device at `192.168.0.115`. Installed `rfid 1.0.4` on that device; APPLaunch service remained active and the transferred package SHA-256 matched. Physical antenna behavior is unverified for this package.
- Source release commit: `c3172da`, pushed to `origin/main`.
- GitHub Release: https://github.com/mtoolstec/CardputerZero-RFID/releases/tag/v1.0.4 (Debian package attached). Application Store update remains pending portal login/submission.
- AppStore portal submission: accepted under `@whywilson`; automated processing run https://github.com/CardputerZero/packages/actions/runs/43322913050 succeeded. Release PR https://github.com/CardputerZero/packages/pull/283 merged on 2026-11-13 (Asia/Shanghai). The portal's My packages page shows `rfid 1.0.4` as "Merged, index rebuilding…"; APT index visibility may lag the merge.
