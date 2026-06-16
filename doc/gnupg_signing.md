# GnuPG Signing in FreeDV GUI

## 1. Overview

FreeDV can attach a digital signature to your transmission using your personal OpenPGP signing key. When you enable signing, FreeDV sends special bookend frames at the start and end of each over-the-air session. A receiving station that has your public key can verify that the callsign in those frames was signed by you and was not altered in transit. The feature is optional and **off by default**. FreeDV uses **signatures only** — it does not encrypt voice or hide content. Anyone can still listen to the audio; signing only helps others confirm who claimed to be on the air.

## 2. Dependencies

The following must be present on your system for signing and verification to work.

### libgpgme

FreeDV links against GPGME (GnuPG Made Easy), the library that talks to GnuPG on your behalf.

| Distribution | Runtime package | Build package |
|--------------|-----------------|---------------|
| Debian / Ubuntu | `libgpgme11` | `libgpgme-dev` |
| Fedora / RHEL | `gpgme` | `gpgme-devel` |
| Arch Linux | `gpgme` | `gpgme` |
| openSUSE | `gpgme` | `gpgme-devel` |

### GnuPG

The `gpg` command-line tool is required. It is already installed on virtually all Linux systems used for amateur radio. Confirm with:

```bash
gpg --version
```

### gpg-agent

GnuPG uses `gpg-agent` as a user daemon to cache passphrases and talk to smart cards. On modern desktop Linux it starts automatically when you first use GnuPG. No manual start is normally needed.

### Hardware token (optional)

If your private key lives on a Nitrokey or other OpenPGP smart card, `scdaemon` (part of the GnuPG suite) handles card communication once the device is inserted.

### Build-time option

FreeDV must be compiled with `-DENABLE_GNUPG_AUTH=ON` for signing support to be included in the binary. Pre-built packages from your distribution may or may not enable this.

To check: open **Options** in FreeDV. If there is no **Signing** tab, your build does not include auth support and you need a build with `ENABLE_GNUPG_AUTH` enabled.

## 3. Finding your existing key fingerprint

A **fingerprint** is a unique 40-character hexadecimal identifier for your OpenPGP key, like a serial number. FreeDV uses it to select which key to sign with.

List keys already in your keyring:

```bash
gpg --list-keys --fingerprint
```

Example output (brainpoolP256r1 key):

```
pub   brainpoolP256r1 2026-03-04 [SC]
      80EF 2F63 B22E 9BC2 6E63  B3DC 8078 8803 ED4C 7A02
uid           [ultimate] LA1ABC <la1abc@example.com>
sub   brainpoolP256r1 2026-03-04 [E]
```

The fingerprint is the long line of hex digits under the `pub` line (`80EF 2F63 ... 7A02`). Spaces are optional when reading it; FreeDV expects the fingerprint **without spaces** in the configuration field:

```
80EF2F63B22E9BC26E63B3DC80788803ED4C7A02
```

To copy a fingerprint without spaces from the first public key in your keyring:

```bash
gpg --list-keys --fingerprint | grep -A1 'pub' | grep -v 'pub' | tr -d ' '
```

## 4. Generating a new key

Create a new key if you have none, or if your existing key cannot sign (no signing-capable subkey).

**Ed25519** is recommended: signatures are small and signing is fast on typical PCs. **brainpoolP256r1** and **RSA** keys also work — FreeDV accepts any OpenPGP key that has a signing-capable subkey.

Start interactive key generation:

```bash
gpg --full-generate-key
```

Walk through the prompts:

1. **Key type:** choose **(9) ECC (sign and encrypt)**, then **(1) Curve 25519** for Ed25519. For RSA instead, choose **(1) RSA and RSA** and pick a size (3072 or 4096 bits).
2. **Expiry:** your choice; one year is a reasonable default. You can extend later.
3. **Real name:** use your callsign (e.g. `LA1ABC`) or your name — your callsign should appear somewhere in the user ID (UID) that will be embedded in signed frames.
4. **Email:** optional but recommended (e.g. `LA1ABC@example.com`). Helps others find your key on keyservers.
5. **Passphrase:** strongly recommended. `gpg-agent` caches it after the first unlock, so you normally enter it once per login session.

Confirm the key was created:

```bash
gpg --list-keys
gpg --list-keys --fingerprint
```

Copy the 40-character fingerprint (no spaces) for use in FreeDV.

## 5. Publishing your key to a keyserver

Publishing your **public** key lets other operators import it without meeting in person. When someone receives your signed transmission, their FreeDV installation can only verify the signature if your public key is in their local keyring. If they do not have it, they will see the key-error icon (`NO_KEY`) instead of the verified icon (`SIGNED`).

Upload your public key (replace `YOUR_FINGERPRINT` with your 40-character fingerprint, no spaces):

```bash
gpg --keyserver hkps://keys.openpgp.org --send-keys YOUR_FINGERPRINT
```

**keys.openpgp.org** sends a verification email to the address in your UID. You must confirm that email before your name/callsign becomes searchable on that server. This reduces spam keys but adds a short delay.

Alternative keyservers:

| Keyserver | Notes |
|-----------|-------|
| `hkps://keyserver.ubuntu.com` | No email verification; UID searchable immediately |
| `hkps://pgp.mit.edu` | Legacy server; widely peered |

Publishing to at least two servers improves reach. Confirm your key is findable by callsign or email:

```bash
gpg --keyserver hkps://keys.openpgp.org --search-keys LA1ABC
```

## 6. Configuring FreeDV

### Signing tab

1. Open **FreeDV** → **Options** → **Signing** tab.
2. Paste your fingerprint (40 hex characters, no spaces) into **GnuPG key fingerprint**.
3. Tick **Enable signing on TX**.
4. Click **OK**.

FreeDV looks up the key immediately. If the secret key is available and has a signing subkey, the log will show a line similar to:

```
FreeDVAuthStep: active signing key 80EF2F63B22E9BC26E63B3DC80788803ED4C7A02 (brainpoolP256r1)
```

If validation fails, a warning dialog appears and the status icon shows key-error.

### Callsign in signed frames

The callsign embedded in auth frames comes from **Options** → **Reporting** → **Callsign** (or the FreeDV text message field if callsign is empty). Set your callsign there before signing on air.

### Hardware token field

If **Hardware token** shows a device name (instead of `none`), FreeDV detected an OpenPGP smart card or Nitrokey. Signing at PTT will use the card automatically.

### Configuration file

Settings are also stored in the FreeDV configuration file:

```
/Signing/EnableSigning/1
/Signing/KeyFingerprint/YOUR_FINGERPRINT
```

### Status bar icons

The auth icon appears on the status bar when the build includes GnuPG auth support. FreeDV updates it from the **UI thread** — RX state is polled periodically while receiving; TX state updates when you key or release PTT.

| Icon | File | Meaning |
|------|------|---------|
| Key | `key.svg` | Signing key loaded and idle; or a new signed transmission has started (START frame seen, END not yet verified) |
| Open padlock | `open.svg` | END frame received and signature verified successfully |
| Unsigned | `auth_unsigned.svg` | No auth frames in the last reception, verification failed, or hash mismatch |
| Key error | `key-error.svg` | Your signing key is missing or unusable (TX); or sender's public key could not be found (RX) |

Typical **receive** sequence:

1. Modem running, no transmission — `key.svg` (if your signing key is configured) or `auth_unsigned.svg`
2. Incoming signed transmission: START frame decoded — `key.svg` (resets any previous result)
3. END frame decoded and signature OK — `open.svg`
4. Sync lost without auth frames — `auth_unsigned.svg`

While you are **transmitting** (PTT keyed), the icon reflects TX signing state via the PTT path and returns to `key.svg` when you release PTT.

## 7. How FreeDV verifies signed transmissions on receive

When another station transmits with signing enabled, FreeDV looks for auth **bookend frames** in the data channel alongside normal voice frames. Auth frames are **variable length**: a fixed 63-byte header (callsign, hash field, signature length) plus an OpenPGP detached signature whose size depends on the key algorithm (typically 70–512 bytes). Large frames are split across several data-channel slots.

This section describes what happens on the **receiving** side.

### When a START auth frame is detected

1. **Callsign recorded** — FreeDV reads the 20-byte callsign field. This is the identity the sender claims on the air.
2. **Session reset** — Payload hashing starts fresh. The status icon returns to `key.svg` (`KEY_LOADED`) so a previous `open.svg` from an earlier transmission is cleared.
3. **No signature check yet** — The START frame is not verified for the status icon. Verification runs when the matching END frame arrives.

While the transmission continues, FreeDV accumulates a SHA-256 hash of decoded speech payload bytes between the START and END frames.

### When an END auth frame is detected

1. **Hash check** — The 32-byte hash in the END frame must match the accumulated payload hash. A mismatch yields `auth_unsigned.svg`.
2. **Signature verification** — FreeDV verifies the detached OpenPGP signature on the END frame prefix (61 bytes, including the hash) using GnuPG.
3. **Key lookup if needed** — If the sender's public key is not in your local keyring, FreeDV attempts to fetch it automatically before retrying verification:

   - Locate by **signing-key fingerprint** embedded in the signature (most reliable)
   - Search the **local keyring** for a key whose user ID contains the callsign in **name**, **comment**, or **email** (case-insensitive)
   - **Keyserver / WKD locate** using the callsign; imported keys must still match the callsign in name, comment, or email

   Successfully imported keys are stored in `~/.gnupg/pubring.kbx` for future QSOs.

   Keyserver behaviour depends on your GnuPG configuration (`~/.gnupg/dirmngr.conf`). If automatic fetch fails (no network, key not published, or email not verified on keys.openpgp.org), import manually:

   ```bash
   gpg --keyserver hkps://keys.openpgp.org --search-keys CALLSIGN
   gpg --keyserver hkps://keys.openpgp.org --recv-keys FINGERPRINT
   ```

### When the modem loses sync

If the receiver loses sync and **no** auth START or END frame was seen during that sync period, the icon is set to `auth_unsigned.svg`. This covers receptions with no signing present.

### Verification result and status icon

| Outcome | Status icon |
|---------|---------------|
| END frame hash OK and signature verified | `open.svg` (SIGNED) |
| END frame seen but public key not found (even after keyserver lookup) | `key-error.svg` (NO_KEY) |
| END frame hash mismatch or signature invalid | `auth_unsigned.svg` (UNSIGNED) |
| Sync lost with no auth frames in that period | `auth_unsigned.svg` (UNSIGNED) |
| START seen, END not yet received | `key.svg` (KEY_LOADED) |

### Practical advice for operators

- Publish your public key to keyservers **before** operating signed FreeDV, so peers' automatic lookup can succeed.
- Put your callsign in the key UID **name**, **comment**, or **email** so callsign-based search matches.
- Exchange keys at hamfests, via email, on QSL cards, or through club mailing lists as a backup.
- Manually imported keys stay in your keyring permanently.

## 8. Hardware token (Nitrokey)

If your **private** signing key is on a Nitrokey or other OpenPGP smart card:

1. Insert the card before starting FreeDV.
2. Open **Options** → **Signing**. The **Hardware token** field should show the card identifier if `scdaemon` detects it. Run `gpg --card-status` in a terminal to troubleshoot if it shows `none`.
3. Signing at PTT start and PTT release is routed through `gpg-agent` and `scdaemon` — no extra FreeDV configuration is required.
4. If the Nitrokey is configured to require physical touch for signing, press the touch button within a few seconds of keying PTT. FreeDV signs on a background thread at PTT edges; a long delay may mean the START frame is sent late or missed by the other station.
5. Nitrokey is a popular choice because its firmware is open source and user-upgradeable, but any OpenPGP-compatible token supported by GnuPG should work.

## 9. Troubleshooting

| Symptom | Likely cause | Action |
|---------|--------------|--------|
| Options → Signing tab missing | Build without `ENABLE_GNUPG_AUTH` | Rebuild with `-DENABLE_GNUPG_AUTH=ON` |
| `key-error.svg` on startup | Fingerprint wrong or secret key not in keyring | Check fingerprint has no spaces; run `gpg --list-secret-keys --fingerprint` |
| `key-error.svg` on RX | Sender's public key not found after automatic lookup | Confirm key is published; check network and `dirmngr`; import manually with `--recv-keys` or `--search-keys` |
| `auth_unsigned.svg` after full PTT cycle | Bad signature, hash mismatch, or missing END frame | Hold PTT long enough for END frame; confirm START and END both sent/received; check sender uses a signing-capable key |
| Nitrokey not shown in Hardware Token field | Card not inserted or `scdaemon` not running | Insert card; run `gpg --card-status` |
| Passphrase prompt blocks PTT | `gpg-agent` cache expired | Unlock once with `gpg --card-status` or `echo test \| gpg --clearsign` before going on air |
| `open.svg` never appears in loopback test | Audio not routed TX → RX, or PTT released before END decoded | Connect line-out to line-in; disable half-duplex; hold PTT several seconds then wait briefly after release for END verification |

### Brief notes

**Missing Signing tab** — Only builds compiled with GnuPG auth support include the feature. Source builds: `cmake -DENABLE_GNUPG_AUTH=ON`.

**Fingerprint errors** — The fingerprint must be exactly 40 hexadecimal characters with no spaces. It identifies the key pair; it is not secret, but it must match a key that has your **secret** signing subkey available locally for TX.

**RX key-error** — Verification uses signatures only; it does not decrypt audio. FreeDV tries automatic keyserver lookup when the sender's public key is missing. If that fails, import the key manually. Publishing your key and including your callsign in the UID helps automatic lookup succeed.

**Loopback testing** — Connect line-out to line-in (or use a virtual cable), run FreeDV in 700D or 700E, enable signing on TX, key PTT for several seconds, then release and wait briefly. The icon should show `key.svg` during TX, then `open.svg` on RX after the END frame is decoded and verified. Disable half-duplex if it mutes RX during TX. Both builds need auth support enabled.
