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

## Signing gate

The current pipeline publishes unsigned releases until HardwareScope has been
accepted by SignPath Foundation and the repository has been connected to its
SignPath project. Do not add placeholder IDs or tokens to `release.yml`.

After acceptance, the release pipeline must submit only GitHub-hosted workflow
artifacts built from the release tag. Signing requests require manual approval.
The HardwareScope application, updater, sensor service, and final installer are
eligible project artifacts. The upstream `PresentMon.exe` binary must remain
outside HardwareScope's signing scope.

After signing is enabled, verify each published executable with
`Get-AuthenticodeSignature` and fail the release if any expected signature is
missing or invalid. Update the signing-status wording in `README.md` only after
that enforcement is active.

If either workflow fails, leave the previous stable manifest unchanged, correct
the source or automation, and rerun the failed workflow.
