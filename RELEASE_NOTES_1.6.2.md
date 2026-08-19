# HardwareScope 1.6.2

HardwareScope 1.6.2 fixes the post-install launch error seen on some Windows PCs.

## Changes

- Fixed Setup error 740 ("The requested operation requires elevation") after an otherwise successful interactive installation.
- The optional **Launch HardwareScope** action on the final Setup page now runs with the same administrator credentials already approved for Setup.
- Silent automatic updates continue to install and relaunch HardwareScope with administrator access.

## Verification

- The installer and application report version 1.6.2.
- Interactive Setup launches HardwareScope through the elevated Inno Setup path instead of the original non-elevated user token.
- A real silent-install test installed 1.6.2 into Program Files and relaunched the application successfully.

HardwareScope 1.6.2 is currently unsigned, so Windows SmartScreen may show an unknown-publisher warning.
