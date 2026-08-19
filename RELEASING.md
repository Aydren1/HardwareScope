# HardwareScope release order

HardwareScope must never announce an update before its installer is publicly downloadable.

## Required order

1. Build the versioned application, installer, release ZIP, and `SHA256SUMS.txt`.
2. Create the GitHub release as a draft and upload every release asset.
3. Publish the stable GitHub release.
4. Let **Publish verified update manifest** download the public installer, validate its SHA-256 checksum, verify that its URL responds, and update `updates/latest.json`.
5. Update the download-page text after the release is public.

Do not manually point `updates/latest.json` at a draft or unpublished release. The release workflow owns the stable updater manifest.

If the workflow fails, keep the previous manifest unchanged. Fix the release assets and rerun the release process; never advertise an unverified installer.
