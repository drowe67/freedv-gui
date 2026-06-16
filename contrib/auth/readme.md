GnuPG auth status bar icons for FreeDV GUI (ENABLE_GNUPG_AUTH).

| File | Status bar meaning |
|------|-------------------|
| key.svg | Key found and loaded — ready to sign (KEY_LOADED) |
| open.svg | Transmission is signed and verified (SIGNED) |
| key-error.svg | Cannot load signing key (NO_KEY) |
| auth_unsigned.svg | Received transmission with no auth frames (UNSIGNED); retained from the original placeholder set — no matching icon was supplied in gnupg-icons |
| locked.svg | Reserved for future encrypted-transmission indication; not used by the status bar yet |

Original notes from gnupg-icons:

- The key symbol signifies that the GnuPG keys are loaded and in the system.
- The open padlock: transmissions are signed.
- The locked padlock: transmissions are encrypted.
- The key with a red X: key error.
