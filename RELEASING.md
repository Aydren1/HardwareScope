# Releasing HardwareScope

HardwareScope releases are built by GitHub Actions from the tagged source. The
stable update manifest is changed only after GitHub has published and verified
the release assets.

## Release process

1. Update the version in `CMakeLists.txt` and add `RELEASE_NOTES_<version>.md`.
2. Merge the tested changes into `main`.
3. Create and push the matching tag, for example `v2.0.0`.
4. The **Build and publish release** workflow builds and tests the Windows x64
   application, installer, portable ZIP, and checksums.
5. The workflow publishes those files as the GitHub release assets.
6. The **Publish verified update manifest** workflow downloads the public
   installer, verifies its name, size, URL, and SHA-256 checksum, then commits
   the new `updates/latest.json` to `main`.

Never point `updates/latest.json` at a draft, local file, or asset that has not
been downloaded and verified from the public GitHub release.

If either workflow fails, leave the previous stable manifest unchanged, correct
the source or automation, and rerun the failed workflow.
