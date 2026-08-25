# Code signing policy

HardwareScope releases are built from tagged source by GitHub Actions. Release
artifacts must pass the project's deterministic tests and must originate from
the public [HardwareScope repository](https://github.com/Cero-SC/HardwareScope).
Every signing request requires manual approval by the project approver.

Free code signing provided by [SignPath.io](https://signpath.io/), certificate
by [SignPath Foundation](https://signpath.org/).

HardwareScope is preparing its SignPath Foundation sponsorship application.
Until a release displays a valid SignPath Foundation signature in Windows file
properties, that release is unsigned. The repository and release notes must not
describe an unsigned build as signed.

## Team roles

- Author, committer, and reviewer: [Cero-SC](https://github.com/Cero-SC)
- Signing approver: [Cero-SC](https://github.com/Cero-SC)
- Automated update-manifest commits: `github-actions[bot]`

Contributions from people who are not committers require review by the project
reviewer before they are merged. Signing approval is separate from building a
release and is never granted automatically.

## Release controls

- Production artifacts are built on GitHub-hosted Windows runners from a
  versioned release tag.
- The source tag and CMake project version must match.
- The application, updater, and sensor service use the same product name and
  product version metadata.
- The upstream PresentMon binary is included under its own license and must not
  be signed as if it were produced by HardwareScope.
- Published files include SHA-256 checksums.
- The stable update manifest is published only after the public installer has
  been downloaded and independently verified by automation.

See the [privacy policy](PRIVACY.md) for network behavior and data handling.
