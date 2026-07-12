# K2 RFID Writer — Flipper Zero FAP

Flipper Zero external app (FAP) for programming Creality K2/K1/HI/CFS filament RFID tags on blank **MIFARE Classic 1K** cards. Uses the same AES encryption and tag layout as the Android, Arduino, and Windows K2-RFID tools in this repository.

## Features

- Configure material (66 Creality K2 filaments), weight, color, printer type, and serial
- **Write Tag** — encrypts sector 1 and writes sector 2
- **Read Tag** — decrypts and displays tag payload
- Works on **official Flipper Zero firmware API 87.1+** and compatible custom builds (Momentum, Unleashed, etc.) — not Momentum-only

## Requirements

- Flipper Zero running **official OFW API 87.1 or newer**, or a custom firmware on the same API (e.g. [Momentum](https://github.com/Next-Flip/Momentum-Firmware), Unleashed)
- Blank MIFARE Classic 1K tags (factory keys `FF FF FF FF FF FF`)
- Two tags per spool (one on each side), identical content

## Pre-built FAP (direct download)

Every push to `main` or `add-flipper-zero-app` that changes `Flipper/` triggers a CI build and publishes **`k2_rfid.fap`** to [GitHub Releases](https://github.com/DnG-Crafts/K2-RFID/releases). Release assets download as a plain `.fap` file — no zip extract step.

**Direct download URL** (use for sideloading and the Flipper Application Catalog):

| Repository | URL |
|------------|-----|
| Upstream | `https://github.com/DnG-Crafts/K2-RFID/releases/download/flipper-fap-latest/k2_rfid.fap` |
| Fork (dev) | `https://github.com/joshs85/K2-RFID/releases/download/flipper-fap-latest/k2_rfid.fap` |

The `flipper-fap-latest` prerelease tag is updated on each successful build from `main` or `add-flipper-zero-app`. Version tags (`v*`, `flipper-*`) also publish a release with the same `k2_rfid.fap` asset name.

> **Note:** GitHub Actions workflow artifacts are always delivered as zip archives and cannot be used for catalog submission. Use the Release URL above.

CI builds against **official Flipper firmware** (pinned commit, currently API **87.1**). That matches Momentum builds on the same API; check **Settings → About** on your Flipper and use a build whose API version matches.

Install the downloaded file:

```text
k2_rfid.fap  →  SD:/ext/apps/NFC/k2_rfid.fap
```

### Flipper Application Catalog

Catalog submission follows [flipper-application-catalog](https://github.com/flipperdevices/flipper-application-catalog) ([Contributing guide](https://github.com/flipperdevices/flipper-application-catalog/blob/main/documentation/Contributing.md)).

**In this repo (already prepared):**

| Item | Location | Status |
|------|----------|--------|
| `application.fam` | `application.fam` | `k2_rfid`, NFC, v1.0, icon |
| Icon (10×10 1-bit PNG) | `assets/icon.png` | Compliant |
| README | `README.md` | Usage + build docs |
| Changelog | `docs/changelog.md` | Catalog format |
| Draft manifest | `catalog/manifest.yml` | Copy into catalog fork |
| Screenshots | `screenshots/` | **You must add qFlipper PNGs** (see `screenshots/README.md`) |

**Direct FAP URL** (sideloading; catalog CI builds from source, not this URL):

```text
https://github.com/DnG-Crafts/K2-RFID/releases/download/flipper-fap-latest/k2_rfid.fap
```

After merge to `main`, use a version tag (e.g. `flipper-v1.0`) for a stable release asset; `flipper-fap-latest` is a rolling prerelease.

**Catalog PR steps:**

1. Merge this app to `DnG-Crafts/K2-RFID` `main`.
2. Capture qFlipper screenshots into `screenshots/` (see `screenshots/README.md`).
3. Update `commit_sha` in `catalog/manifest.yml` to the merged commit on `main`.
4. Fork [flipper-application-catalog](https://github.com/flipperdevices/flipper-application-catalog), branch `youruser/k2_rfid_1.0`.
5. Add `applications/NFC/k2_rfid/manifest.yml` (contents from `catalog/manifest.yml`).
6. Validate locally: `python3 tools/bundle.py --nolint applications/NFC/k2_rfid/manifest.yml bundle.zip`
7. Open PR using the catalog template.

**Metadata** (from `application.fam`; catalog reads these from source):

- **App ID:** `k2_rfid`
- **Name:** K2 RFID Writer
- **Category:** NFC
- **Version:** 1.0
- **Author:** DnG-Crafts / Flipper port
- **Short description:** Write Creality K2/K1/CFS MIFARE Classic filament tags
- **API version:** must match CI build (see `flipper-fap-latest` release notes; currently **87.1**)

## Build & Install (local)

Build against the same firmware API version as your device (check **Settings → About**). The example below uses Momentum; official OFW or Unleashed work the same way if the API matches.

1. Clone firmware (example: Momentum — must match the version on your Flipper):

```bash
git clone --recursive https://github.com/Next-Flip/Momentum-Firmware.git
cd Momentum-Firmware
```

2. Copy this folder into `applications_user`:

```bash
cp -r /path/to/K2-RFID/Flipper applications_user/k2_rfid
```

3. Build the FAP:

```bash
./fbt fap_k2_rfid
```

4. Install the output file on SD card:

```text
build/f7-firmware-D/.extapps/k2_rfid.fap  →  SD:/ext/apps/NFC/k2_rfid.fap
```

Or build and launch over USB:

```bash
./fbt launch APPSRC=applications_user/k2_rfid
```

**Important:** Build against the same firmware version running on your device so the FAP API version matches.

## Usage

1. Open **Apps → NFC → K2 RFID Writer**
2. **Configure Tag** — cycle weight/printer; OK to pick material or color (presets + custom hex); edit serial
3. **Write Tag** — hold a blank tag on the back; repeat for the second spool tag
4. **Read Tag** — verify an existing tag

## Tag format

ASCII payload (96 bytes), same layout as other K2-RFID tools:

```text
AB124 + 0276 + A2 + 1{materialId} + 0{color} + {length} + {serial} + reserve + {printer}
```

Sector 1 blocks 4–6 are AES-encrypted; sector 1 trailer receives a UID-derived key on first write.
