# HardwareScope 1.6.3

HardwareScope 1.6.3 makes automatic and manual updates reliable when they are triggered close together or the network interrupts a download.

## Changes

- Prevented automatic and manual update checks from downloading the same installer concurrently.
- Added a process-wide download gate and unique temporary files so update attempts cannot overwrite each other.
- Added up to three automatic download attempts with cache bypassing.
- Added exact installer-size validation before SHA-256 verification.
- Extended the download timeout for slower connections.
- Checksum failures now report the expected and received hashes for diagnosis while continuing to reject altered files.
- Retains the Setup error 740 fix from version 1.6.2.

## Verification

- The live GitHub 1.6.2 installer independently downloaded as 76,290,919 bytes and matched its published SHA-256 checksum.
- HardwareScope's own downloader successfully fetched and verified the same release.
- Two simultaneous update downloads were serialized and both passed size and SHA-256 verification.
- The installer and application report version 1.6.3.

HardwareScope 1.6.3 is currently unsigned, so Windows SmartScreen may show an unknown-publisher warning.
