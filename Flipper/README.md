# K2 RFID Writer — Flipper Zero FAP

Flipper Zero external app (FAP) for programming Creality K2/K1/HI/CFS filament RFID tags on blank **MIFARE Classic 1K** cards. Uses the same AES encryption and tag layout as the Android, Arduino, and Windows K2-RFID tools in this repository.

## Features

- Configure material (66 Creality K2 filaments), weight, color, printer type, and serial
- **Write Tag** — encrypts sector 1 and writes sector 2
- **Read Tag** — decrypts and displays tag payload
- Works as a standard FAP on **Momentum** firmware (and other builds with a compatible NFC API)

## Requirements

- Flipper Zero with [Momentum Firmware](https://github.com/Next-Flip/Momentum-Firmware)
- Blank MIFARE Classic 1K tags (factory keys `FF FF FF FF FF FF`)
- Two tags per spool (one on each side), identical content

## Pre-built FAP (GitHub Actions)

Every push to `main` or `add-flipper-zero-app` that changes `Flipper/` triggers a CI build.

1. Open the repository **Actions** tab → **Build Flipper FAP** → latest green run.
2. Download the workflow **artifact** (`.fap` files, named with API version).
3. On pushes to `main` or version tags, the same `.fap` is also attached to [GitHub Releases](https://github.com/DnG-Crafts/K2-RFID/releases) (`flipper-fap-latest` for main, or the tag name).

CI builds against **official Flipper firmware** `release` (pinned commit, currently API **87.1**). That matches Momentum builds on the same API; check **Settings → About** on your Flipper and use a build whose API version matches.

Install the downloaded file:

```text
k2_rfid.fap  →  SD:/ext/apps/NFC/k2_rfid.fap
```

## Build & Install (local)

1. Clone Momentum firmware (must match the version on your Flipper):

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
