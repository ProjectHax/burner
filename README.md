<p align="center">
  <img src="packaging/burner.svg" alt="Burner" width="128" height="128">
</p>

<h1 align="center">Burner</h1>

<p align="center">
  A modern CD/DVD/Blu-ray burning application for Linux built with Qt6 and C++20.
</p>

<p align="center">
  <a href="#features">Features</a> •
  <a href="#installation">Installation</a> •
  <a href="#building">Building</a> •
  <a href="#translations">Translations</a> •
  <a href="#testing">Testing</a> •
  <a href="#license">License</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Qt-6.2+-41cd52?logo=qt" alt="Qt 6.2+">
  <img src="https://img.shields.io/badge/C++-20-00599C?logo=cplusplus" alt="C++20">
  <img src="https://img.shields.io/badge/License-GPLv3-blue" alt="GPLv3 License">
  <img src="https://img.shields.io/badge/Platform-Linux-orange?logo=linux" alt="Linux">
</p>

---

## Features

- **Data Disc**: Create data CDs/DVDs with ISO 9660 filesystem
- **Audio CD**: Burn audio CDs from various audio formats (MP3, FLAC, WAV, OGG, etc.)
- **Video CD**: Create VCD and SVCD discs
- **Disc Image Writer**: Write ISO, BIN/CUE, and other disc images to disc
- **Disc Cloner**: Clone discs to ISO or directly to another disc
- **CD Ripper**: Rip audio CDs to FLAC, MP3, AAC, OGG, or WAV with MusicBrainz metadata lookup
- **Project Files**: Save and load burn projects (.burn files)
- **Verification**: Optional burn verification to ensure data integrity
- **Multi-language**: Localized interface with 15 language translations

## Installation

### Pre-built Packages

Download the latest release from the [Releases](../../releases) page:

| Package | Platform |
|---------|----------|
| `Burner-x.x.x-x86_64.AppImage` | Most distributions from 2021 on (glibc 2.34+) |
| `Burner-x.x.x.flatpak` | Any Linux (sandboxed) |
| `burner-x.x.x-1.el10.x86_64.rpm` | RHEL 10 / Rocky 10 / AlmaLinux 10 |
| `burner-x.x.x-1.el9.x86_64.rpm` | RHEL 9 / Rocky 9 / AlmaLinux 9 |
| `burner_x.x.x-1-ubuntu24.04_amd64.deb` | Ubuntu 24.04 |
| `burner_x.x.x-1-ubuntu22.04_amd64.deb` | Ubuntu 22.04 |
| `burner_x.x.x-1-debian13_amd64.deb` | Debian 13 |
| `burner_x.x.x-1-debian12_amd64.deb` | Debian 12 |

### Flatpak

```bash
flatpak install --user ./Burner-*.flatpak
flatpak run com.github.projecthax.Burner
```

### AppImage

```bash
chmod +x Burner-*-x86_64.AppImage
./Burner-*-x86_64.AppImage
```

### RPM (RHEL / Rocky / AlmaLinux 9 and 10)

Qt 6 and FFmpeg come from EPEL, so enable it first:

```bash
sudo dnf install epel-release
sudo dnf install ./burner-*.el10.x86_64.rpm   # or .el9 on version 9
```

### DEB (Ubuntu / Debian)

Use the package matching your release:

```bash
sudo apt install ./burner_*-ubuntu24.04_amd64.deb   # or -ubuntu22.04, -debian13, -debian12
```

## Building from Source

### Build Dependencies

- CMake 3.21+
- Qt 6.2+ (Core, Gui, Widgets, LinguistTools)
- libburn, libisofs, libisoburn (libburnia project)
- FFmpeg libraries (libavcodec, libavformat, libavutil, libswresample, libswscale)
- libcurl (for MusicBrainz metadata lookup)
- OpenSSL (for disc ID calculation)
- C++20 compatible compiler (GCC 10+, Clang 12+)

### Runtime Dependencies

- Linux with optical drive support
- User must have read/write access to optical devices (typically via `cdrom` group)

## Installing Dependencies

### Fedora / RHEL 9+ / CentOS Stream / Rocky / AlmaLinux

```bash
sudo dnf install cmake gcc-c++ qt6-qtbase-devel qt6-qttools-devel qt6-qtwayland-devel \
    libburn-devel libisofs-devel libisoburn-devel \
    ffmpeg-free-devel libcurl-devel openssl-devel
```

Note: On RHEL/Rocky/AlmaLinux, enable EPEL and CRB repositories first:
```bash
sudo dnf install epel-release
sudo dnf config-manager --set-enabled crb
```

### Debian / Ubuntu / Linux Mint

```bash
sudo apt update
sudo apt install build-essential cmake qt6-base-dev qt6-tools-dev qt6-tools-dev-tools \
    qt6-l10n-tools libgl1-mesa-dev \
    libburn-dev libisofs-dev libisoburn-dev \
    libavcodec-dev libavformat-dev libavutil-dev libswresample-dev libswscale-dev \
    libcurl4-openssl-dev libssl-dev
```

### Arch Linux / Manjaro

```bash
sudo pacman -S base-devel cmake qt6-base qt6-tools \
    libburn libisofs libisoburn \
    ffmpeg curl openssl
```

### openSUSE Tumbleweed / Leap 15.5+

```bash
sudo zypper install cmake gcc-c++ qt6-base-devel qt6-tools-devel qt6-linguist-devel \
    libburn-devel libisofs-devel libisoburn-devel \
    ffmpeg-6-libavcodec-devel ffmpeg-6-libavformat-devel ffmpeg-6-libavutil-devel \
    ffmpeg-6-libswresample-devel ffmpeg-6-libswscale-devel \
    libcurl-devel libopenssl-devel
```

For Leap, enable Packman repository for FFmpeg:
```bash
sudo zypper addrepo -cfp 90 'https://ftp.gwdg.de/pub/linux/misc/packman/suse/openSUSE_Leap_$releasever/' packman
sudo zypper dup --from packman --allow-vendor-change
```

### Gentoo

```bash
sudo emerge --ask dev-build/cmake dev-qt/qtbase:6 dev-qt/qttools:6 \
    dev-libs/libburn dev-libs/libisofs dev-libs/libisoburn \
    media-video/ffmpeg net-misc/curl dev-libs/openssl
```

### Alpine Linux

```bash
sudo apk add cmake g++ qt6-qtbase-dev qt6-qttools-dev \
    libburn-dev libisofs-dev libisoburn-dev \
    ffmpeg-dev curl-dev openssl-dev
```

### Void Linux

```bash
sudo xbps-install -S cmake gcc qt6-base-devel qt6-tools-devel \
    libburn-devel libisofs-devel libisoburn-devel \
    ffmpeg-devel libcurl-devel libressl-devel
```

## Building

```bash
# Create build directory
mkdir build && cd build

# Configure
cmake ..

# Build
make -j$(nproc)

# Run
./src/burner
```

### Building with Qt Creator

Open `burner.pro` in Qt Creator and build normally.

## Installation

```bash
# From build directory
sudo make install
```

## Translations

The application supports the following languages:
- English (en)
- Spanish (es)
- French (fr)
- German (de)
- Italian (it)
- Portuguese (pt)
- Russian (ru)
- Chinese Simplified (zh_CN)
- Chinese Traditional (zh_TW)
- Japanese (ja)
- Korean (ko)
- Arabic (ar)
- Dutch (nl)
- Polish (pl)
- Turkish (tr)

### Updating Translations

```bash
# Extract strings from source
./scripts/update-sources.sh

# Translate a specific language
./scripts/translate-language.sh es

# Translate all languages
./scripts/translate-all.sh

# Compile translations
./scripts/compile-translations.sh

# Check translation status
./scripts/validate-translations.sh
```

## Project Structure

```
burner/
├── src/
│   ├── core/           # Core burning logic (non-Qt)
│   │   └── libburnia/  # libburn/libisofs wrappers
│   ├── engine/         # Qt integration layer
│   ├── models/         # Qt item models
│   └── ui/             # User interface
│       ├── widgets/    # Reusable widgets
│       ├── pages/      # Main tab pages
│       └── dialogs/    # Dialog windows
├── translations/       # Qt translation files (.ts/.qm)
├── scripts/            # Build and translation scripts
└── cmake/              # CMake modules
```

## Testing

This guide provides comprehensive testing procedures for verifying Burner functionality.

### Prerequisites

**Required Hardware:**
- Optical drive (CD/DVD/BD burner recommended)
- Blank discs for burn testing (CD-R, CD-RW, DVD-R, or DVD+RW)
- An audio CD (for ripping tests)

**Required Files:**
- Several test files/folders (for data disc testing)
- Audio files: MP3, FLAC, WAV, OGG (at least one of each)
- Video files: MPEG-1 or MPEG-2 (for VCD/SVCD testing)
- ISO image file (any bootable or data ISO)
- BIN/CUE disc image (optional, for multi-track testing)

**Optional:**
- Second optical drive (for disc-to-disc cloning)
- Multiple disc types for comprehensive media testing

---

### 1. Application Launch & Basic UI

| Test | Steps | Expected Result |
|------|-------|-----------------|
| Application starts | Run `./build/src/burner` | Window appears with 7 tabs |
| Tabs visible | Check tab bar | Data CD, Audio CD, Video CD, Burn ISO, Clone Disc, Rip CD, Browse ISO |
| Drive selector | Check toolbar | Drive dropdown shows available optical drives |
| Capacity bar | Check bottom of window | Shows disc capacity (or "No disc" if empty) |
| Menu bar | Check top | File, View, Tools, Help menus present |
| Status bar | Check bottom | Status message visible |

---

### 2. Drive Management

| Test | Steps | Expected Result |
|------|-------|-----------------|
| Drive detection | Open application with drive connected | Drive appears in dropdown |
| Refresh drives | Press F5 or Tools → Refresh Drives | Drive list updates |
| Eject disc | Tools → Eject (with disc inserted) | Disc tray opens |
| Media detection | Insert disc, wait 2-3 seconds | Capacity bar updates with disc info |
| No disc handling | Eject disc | Shows "No disc" or "No media" |
| Multiple drives | Connect multiple drives | All drives appear in dropdown |

---

### 3. Data Disc Page (Tab 1)

| Test | Steps | Expected Result |
|------|-------|-----------------|
| Add files | Click "Add Files" → select files | Files appear in tree view |
| Add folder | Click "Add Folder" → select folder | Folder with contents appears in tree |
| Create folder | Click "Create Folder" → enter name | New folder created in tree |
| Remove selected | Select item → click "Remove Selected" | Item removed from tree |
| Clear all | Click "Clear All" | Tree cleared, size shows 0 |
| Drag-drop files | Drag files from file manager to page | Files added to tree |
| Size calculation | Add multiple files | Total size updates correctly |
| Capacity warning | Add files exceeding disc capacity | Capacity bar turns red |
| Save as ISO | Add files → click "Save as ISO" | ISO file created at chosen location |
| Verify ISO | Open saved ISO with file manager or ISO Browser | Contains all added files |

---

### 4. Audio CD Page (Tab 2)

| Test | Steps | Expected Result |
|------|-------|-----------------|
| Add tracks | Click "Add Tracks" → select audio files | Tracks appear in list with durations |
| MP3 support | Add .mp3 file | Track added, duration shown |
| FLAC support | Add .flac file | Track added, duration shown |
| WAV support | Add .wav file | Track added, duration shown |
| OGG support | Add .ogg file | Track added, duration shown |
| Track reorder up | Select track → click "Move Up" | Track moves up in list |
| Track reorder down | Select track → click "Move Down" | Track moves down in list |
| Remove track | Select track → click "Remove Selected" | Track removed from list |
| Clear all | Click "Clear All" | All tracks removed |
| Duration total | Add multiple tracks | Total duration displayed (e.g., "45:30 / 80:00") |
| 80-minute warning | Add tracks exceeding 80 minutes | Warning displayed |
| Drag-drop audio | Drag audio files to page | Tracks added |

---

### 5. Video CD Page (Tab 3)

| Test | Steps | Expected Result |
|------|-------|-----------------|
| Add videos | Click "Add Videos" → select MPEG files | Videos appear in list |
| VCD format | Select "VCD" from dropdown | Format info shows VCD specs |
| SVCD format | Select "SVCD" from dropdown | Format info shows SVCD specs |
| Remove video | Select video → click "Remove Selected" | Video removed |
| Clear all | Click "Clear All" | All videos removed |
| Format requirements | Check info label | Shows format requirements |

---

### 6. Burn ISO Page (Tab 4)

| Test | Steps | Expected Result |
|------|-------|-----------------|
| Browse ISO | Click "Browse" → select ISO file | ISO info displayed |
| ISO info display | Load ISO file | Shows volume label, size, type |
| BIN/CUE support | Load .cue file | Shows track list from CUE |
| Drag-drop ISO | Drag ISO file to page | ISO loaded and info displayed |
| Clear ISO | Click "Clear" | ISO cleared, info hidden |
| MD5 calculation | Load ISO | MD5 hash calculated and displayed |
| SHA-256 calculation | Load ISO | SHA-256 hash calculated and displayed |
| Copy MD5 | Click copy button next to MD5 | Hash copied to clipboard |
| Copy SHA-256 | Click copy button next to SHA-256 | Hash copied to clipboard |
| Verify hash (match) | Enter correct hash → click "Verify" | Shows checksum match (green) |
| Verify hash (mismatch) | Enter wrong hash → click "Verify" | Shows checksum mismatch (red) |
| Disc recommendation | Load various size ISOs | Recommends appropriate disc type |

---

### 7. Clone Disc Page (Tab 5)

**Clone to Image:**

| Test | Steps | Expected Result |
|------|-------|-----------------|
| Clone to ISO | Insert disc → select ISO format → click "Clone" | ISO file created |
| Clone to BIN/CUE | Insert disc → select BIN/CUE → click "Clone" | BIN and CUE files created |
| Clone progress | Start clone operation | Progress bar shows read progress |
| Clone cancel | Click "Cancel" during clone | Operation stops gracefully |

**Blank Disc:**

| Test | Steps | Expected Result |
|------|-------|-----------------|
| Quick blank | Insert RW disc → select "Quick Blank" → click "Blank" | Disc blanked quickly |
| Full blank | Insert RW disc → select "Full Blank" → click "Blank" | Disc fully blanked (slower) |
| Blank progress | Start blank operation | Progress dialog shows blanking |

**Integrity Check:**

| Test | Steps | Expected Result |
|------|-------|-----------------|
| Check integrity | Insert disc → click "Check Integrity" | Reads and verifies all sectors |
| Stop on error | Enable "Stop on First Error" → check damaged disc | Stops at first bad sector |
| Full check | Disable "Stop on First Error" → check disc | Completes full check, reports errors |

---

### 8. CD Ripper Page (Tab 6)

| Test | Steps | Expected Result |
|------|-------|-----------------|
| Disc detection | Insert audio CD → wait | Tracks detected and listed |
| Refresh tracks | Click "Refresh" | Track list updates |
| Metadata lookup | Insert known album | Artist/album/track names populated |
| Cover art | Insert known album | Album cover displayed (if available) |
| Select all tracks | Click "Select All" | All tracks checked |
| Deselect all | Click "Deselect All" | All tracks unchecked |
| Select individual | Click checkbox on single track | Only that track selected |
| Format selection | Change output format dropdown | Format changes |
| Quality selection | Change quality dropdown | Quality setting changes |
| Output directory | Click "Browse" → select folder | Path updated |
| Filename pattern | Modify pattern field | Preview updates |
| Pattern presets | Select from presets dropdown | Pattern field updates |
| Rip selected | Select tracks → click "Rip" | Progress shown, files created |
| Rip cancel | Click "Cancel" during rip | Operation stops |
| Verify output | Check output folder | Audio files present with correct format |
| Embed cover | Enable "Embed Cover" → rip | Audio file contains cover art |
| Save cover | Enable "Save Cover" → rip | Cover image saved separately |

---

### 9. ISO Browser Page (Tab 7)

| Test | Steps | Expected Result |
|------|-------|-----------------|
| Open ISO | Click "Browse" → select ISO | File tree displayed |
| Drag-drop ISO | Drag ISO to page | ISO loaded, tree displayed |
| Navigate tree | Expand/collapse folders | Directory structure visible |
| Volume info | Load ISO | Volume label, size, file count shown |
| Extract file | Select file → click "Extract Selected" | File extracted to chosen location |
| Extract folder | Select folder → click "Extract Selected" | Folder and contents extracted |
| Extract all | Click "Extract All" | All contents extracted |
| Clear ISO | Click "Clear" | Tree cleared, info hidden |
| Large ISO | Open ISO with many files | Tree loads and displays correctly |

---

### 10. Burn Options Dialog

| Test | Steps | Expected Result |
|------|-------|-----------------|
| Dialog opens | Click "Burn Disc" on any page with content | Options dialog appears |
| Volume label | Enter custom volume label | Label accepted (max 32 chars) |
| Speed selection | Click speed dropdown | Shows available speeds from drive |
| Simulate mode | Enable "Simulate" checkbox | Dry-run mode enabled |
| Verify after burn | Enable "Verify After Burn" | Verification will run after write |
| Eject after burn | Toggle "Eject After" | Setting changes |
| Buffer protection | Toggle "Buffer Underrun Protection" | Setting changes |
| Leave disc open | Toggle "Leave Disc Open" | Multi-session setting changes |
| Blank first | Toggle "Blank Disc First" (RW media) | Pre-burn blank enabled |
| Cancel dialog | Click "Cancel" | Dialog closes, burn not started |
| Proceed with burn | Click "OK" | Burn starts with selected options |

---

### 11. Burn Progress Dialog

| Test | Steps | Expected Result |
|------|-------|-----------------|
| Progress display | Start burn | Progress bar updates smoothly |
| Percentage | Watch during burn | Percentage increases 0-100% |
| Time display | Watch during burn | Elapsed and remaining time shown |
| Speed display | Watch during burn | Write speed shown (e.g., "8x DVD") |
| Buffer display | Watch during burn (data mode) | Buffer fill percentage shown |
| Log toggle | Click log expand/collapse | Log section expands/collapses |
| Log entries | Watch during burn | Timestamped entries appear |
| Log colors | Check log entries | Different colors for info/warning/error |
| Cancel burn | Click "Cancel" during burn | Burn stops with confirmation |
| Completion | Let burn complete | Success message shown |
| Close button | After completion | "Close" button appears |
| Auto-eject | Complete burn with "Eject After" enabled | Disc ejected automatically |
| Verification | Complete burn with "Verify" enabled | Verification runs after write |

---

### 12. Checksum Dialog (Tools Menu)

| Test | Steps | Expected Result |
|------|-------|-----------------|
| Open dialog | Tools → Checksum Calculator | Dialog opens |
| Browse file | Click "Browse" → select file | File path shown |
| Calculate | Click "Calculate" | Progress bar shows, hashes calculated |
| MD5 result | After calculation | MD5 hash displayed |
| SHA-256 result | After calculation | SHA-256 hash displayed |
| Copy MD5 | Click "Copy MD5" | Hash copied to clipboard |
| Copy SHA-256 | Click "Copy SHA-256" | Hash copied to clipboard |
| Verify correct | Enter correct hash → click "Verify" | Match confirmed (green) |
| Verify incorrect | Enter wrong hash → click "Verify" | Mismatch shown (red) |
| Large file | Select file >1GB | Progress shown, completes successfully |

---

### 13. Project File Management

| Test | Steps | Expected Result |
|------|-------|-----------------|
| Save project | File → Save Project (Ctrl+S) | .burn file created |
| Save as | File → Save Project As (Ctrl+Shift+S) | Save dialog, new file created |
| Open project | File → Open Project (Ctrl+O) | Project loaded, content restored |
| New project | File → New Project (Ctrl+N) | Current content cleared |
| Recent projects | File → Recent Projects | Shows previously opened projects |
| Clear recent | File → Recent Projects → Clear | Recent list emptied |
| Unsaved warning | Modify project → File → Exit | Prompts to save changes |
| Project formats | Save/open for each page type | All page types persist correctly |

---

### 14. Theme System

| Test | Steps | Expected Result |
|------|-------|-----------------|
| System theme | View → Theme → System | Follows system dark/light mode |
| Light theme | View → Theme → Light | UI uses light colors |
| Dark theme | View → Theme → Dark | UI uses dark colors |
| Theme persistence | Change theme → restart app | Theme selection preserved |
| UI readability | Check all UI elements in each theme | Text and icons visible |

---

### 15. Keyboard Shortcuts

| Shortcut | Action | Expected Result |
|----------|--------|-----------------|
| Ctrl+N | New Project | Content cleared |
| Ctrl+O | Open Project | Open dialog appears |
| Ctrl+S | Save Project | Project saved |
| Ctrl+Shift+S | Save As | Save dialog appears |
| Ctrl+Q | Exit | Application closes |
| F5 | Refresh Drives | Drive list updates |

---

### 16. Error Handling

| Test | Steps | Expected Result |
|------|-------|-----------------|
| No drive | Start app without optical drive | Graceful message, UI functional |
| No disc | Try to burn without disc | Clear error message |
| Full disc | Try to add more than disc capacity | Red capacity bar, burn prevented |
| Invalid file | Try to load corrupted ISO | Error message displayed |
| Permission denied | Try to burn without drive permissions | Helpful error message |
| Busy drive | Try to access mounted/busy drive | Warning about busy state |

---

### 17. Burn Tests (Requires Blank Media)

**Warning:** These tests consume blank media. Use CD-RW/DVD-RW if available to reuse discs.

| Test | Media | Steps | Expected Result |
|------|-------|-------|-----------------|
| Data CD | CD-R/RW | Add files → burn | Files readable on burned disc |
| Data DVD | DVD-R/RW | Add files → burn | Files readable on burned disc |
| Audio CD | CD-R | Add audio tracks → burn | Plays in CD player |
| ISO write | CD/DVD | Load ISO → burn | ISO contents on disc |
| Simulate | Any RW | Enable simulate → burn | Completes without writing |
| Verify | Any | Enable verify → burn | Verification passes |
| Multi-session | CD/DVD-RW | Burn → burn again with "Leave Open" | Both sessions readable |

---

### Test Completion Checklist

After completing all tests, verify:

- [ ] All 7 tabs function correctly
- [ ] Drive detection and management works
- [ ] All file operations (add/remove/clear) work
- [ ] All supported audio formats load
- [ ] ISO reading and browsing works
- [ ] Checksum calculation is accurate
- [ ] Project save/load preserves data
- [ ] Keyboard shortcuts function
- [ ] Theme switching works
- [ ] Error messages are helpful
- [ ] At least one successful burn (with media)

## License

Copyright (C) 2026 ProjectHax LLC

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

See [LICENSE](LICENSE) for the full license text.

## Releasing

Every push to `master` and every pull request is built by GitHub Actions, which produces the RPM, DEB, AppImage and Flatpak packages as downloadable workflow artifacts.

The version number lives in the `VERSION` file and is used by the build, the packages and the About dialog. To publish a release, update it, commit, and push a matching tag:

```bash
echo 0.2.0 > VERSION
git commit -am "Release 0.2.0"
git tag v0.2.0
git push origin master v0.2.0
```

The workflow builds all packages, then creates a GitHub release with the packages, a `SHA256SUMS.txt` file, and a changelog generated from the commits since the previous release. The build fails if the tag doesn't match `VERSION`. Tags with a suffix (`v0.2.0-rc1`) are published as pre-releases.

Commit subjects starting with `feat:`, `fix:` or `perf:` (optionally with a scope, e.g. `fix(ripper): ...`) are grouped into Features, Bug Fixes and Performance sections; all other commits are listed as-is. To preview the notes for the next release:

```bash
./scripts/changelog.sh
```

## Contributing

Contributions are welcome! Please ensure your code:
- Follows the existing code style
- Compiles without warnings
- Includes appropriate error handling
- Updates translations if adding user-visible strings

## Acknowledgments

- [libburnia project](https://dev.lovelyhq.com/libburnia) for libburn, libisofs, and libisoburn
- [Qt Project](https://www.qt.io/) for the Qt framework
- [FFmpeg](https://ffmpeg.org/) for audio encoding/decoding
- [MusicBrainz](https://musicbrainz.org/) for CD metadata lookup API
- [libcurl](https://curl.se/libcurl/) for HTTP client functionality
- [OpenSSL](https://www.openssl.org/) for cryptographic functions
- [nlohmann/json](https://github.com/nlohmann/json) for JSON parsing (MIT License)
