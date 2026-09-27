# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Build with CMake (recommended)
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run the application
./src/burner

# Install
sudo make install
```

Alternative build with Qt Creator: Open `burner.pro` and build normally.

## Packaging

After building, generate distribution packages:

```bash
./scripts/package.sh appimage   # Build AppImage
./scripts/package.sh rpm        # Build RPM for RHEL 10 / Fedora
./scripts/package.sh deb        # Build .deb for Debian/Ubuntu
./scripts/package.sh all        # Build all packages
./scripts/package.sh check      # Check prerequisites
```

Packages are output to:
- AppImage: `build/Burner-<version>-x86_64.AppImage`
- RPM: `build/rpmbuild/RPMS/x86_64/burner-<version>-1.el10.x86_64.rpm`
- SRPM: `build/rpmbuild/SRPMS/burner-<version>-1.el10.src.rpm`
- DEB: `build/burner_<version>-1~<distro><release>_amd64.deb` (e.g. `-1~ubuntu24.04`; plain `-1` when cross-built with fpm on RHEL)

### CI and Releases

`.github/workflows/build.yml` builds all packages on every push to `master` and on pull requests: RPMs for EL9 and EL10, DEBs for Ubuntu 22.04, 24.04 and Debian 12, 13 (each in a container of that distro), an AppImage built on EL9 (the oldest target, so it runs on newer distros), and a Flatpak. Keep the code building against the oldest toolchain: Qt 6.2 and FFmpeg 4.4 (Ubuntu 22.04), GCC 11 (EL9); `src/core/FFmpegCompat.hpp` covers the FFmpeg channel layout API differences. Pushing a `v*` tag also creates a GitHub release whose notes come from `./scripts/changelog.sh <tag>` (commits since the previous tag, grouped by `feat:`/`fix:`/`perf:` prefixes). The version lives in the `VERSION` file (read by CMake, which defines `BURNER_VERSION` for the code, and by the packaging scripts); the release workflow fails if the tag doesn't match it.

### RPM Build Requirements (RHEL 10)

```bash
sudo dnf install rpm-build rpmdevtools
sudo dnf install qt6-qtbase-devel qt6-qttools-devel qt6-qtwayland-devel
sudo dnf install libburn-devel libisofs-devel libisoburn-devel
sudo dnf install ffmpeg-free-devel
```

### DEB Build Requirements (cross-build on RHEL)

Uses [fpm](https://fpm.readthedocs.io/) to create .deb packages on any Linux:

```bash
sudo dnf install ruby ruby-devel gcc make rpm-build
gem install fpm
```

### AppImage Requirements

```bash
sudo dnf install fuse fuse-libs curl
# linuxdeploy is downloaded automatically by the script
```

## Translation Scripts

```bash
./scripts/update-sources.sh        # Extract translatable strings from source
./scripts/translate-language.sh es # Translate specific language
./scripts/compile-translations.sh  # Compile .ts to .qm files
./scripts/validate-translations.sh # Check translation status
```

## Architecture

The codebase follows a layered architecture separating Qt UI from core burning logic:

```
UI Layer (src/ui/)
    ↓
Qt Integration (src/engine/) - QtBurnEngine, DriveMonitor, BurnWorker
    ↓
Qt Models (src/models/) - DriveListModel, FileTreeModel, TrackListModel
    ↓
Core Layer (src/core/) - Pure C++20, no Qt dependencies
    ├── BurnEngine - High-level disc burning operations
    ├── DriveManager - Drive discovery and control
    ├── ImageBuilder - ISO image construction
    ├── AudioEncoder - Audio format conversion via FFmpeg
    ├── DiscCloner - Disc cloning operations
    └── libburnia/ - RAII wrappers for libburn/libisofs
```

**Key design principle**: Core logic in `src/core/` has no Qt dependencies and can be tested independently. Qt integration happens through the `src/engine/` layer using signals/slots and worker threads.

## Key Entry Points

- `src/main.cpp` - Application initialization, translation loading, dark mode detection
- `src/ui/MainWindow.hpp` - Main window with tabbed interface for 5 burning modes
- `src/core/BurnEngine.hpp` - Core burning operations (data, audio, ISO)
- `src/engine/QtBurnEngine.hpp` - Qt wrapper with async worker thread

## CMake Library Structure

The project builds as 4 static libraries plus the main executable:
- `burner_core` - Pure C++20 burning logic
- `burner_engine` - Qt integration layer
- `burner_models` - Qt item models
- `burner_ui` - User interface components

## Code Style

- C++20 with modern features (smart pointers, std::filesystem, std::atomic)
- RAII for resource management (see BurniaInit for libburnia lifecycle)
- Pimpl pattern for Qt classes (see QtBurnEngine)
- Compiler flags: `-Wall -Wextra -Wpedantic -Werror=return-type`

## Translation Files

Translation files are in `translations/burner_<lang>.ts` (Qt Linguist XML format).

**Claude is authorized to directly edit translation files** in `translations/*.ts`.

### Translation File Format

```xml
<message>
    <source>English text</source>
    <translation type="unfinished"></translation>  <!-- needs translation -->
</message>

<message>
    <source>English text</source>
    <translation>Translated text</translation>      <!-- completed -->
</message>
```

### Translation Guidelines

When translating or editing `.ts` files:

1. **Translate `<translation>` content only** - never modify `<source>`, `<name>`, or `<location>` elements
2. **Remove `type="unfinished"`** when providing a translation
3. **Preserve placeholders**: `%1`, `%2`, `%n` (numbered arguments)
4. **Preserve HTML**: `<b>`, `<br/>`, `&lt;`, etc.
5. **Preserve keyboard shortcuts**: `&` prefix (e.g., `&File` → `&Archivo`)
6. **Keep technical terms**: CD, DVD, ISO, VCD, SVCD (or use standard localizations)
7. **Be concise**: UI labels should fit in buttons/menus

### Supported Languages

| Code   | Language              |
|--------|-----------------------|
| en     | English (source)      |
| es     | Spanish               |
| fr     | French                |
| de     | German                |
| it     | Italian               |
| pt     | Portuguese            |
| ru     | Russian               |
| zh_CN  | Chinese (Simplified)  |
| zh_TW  | Chinese (Traditional) |
| ja     | Japanese              |
| ko     | Korean                |
| ar     | Arabic                |
| nl     | Dutch                 |
| pl     | Polish                |
| tr     | Turkish               |

After editing, run `./scripts/compile-translations.sh` to generate `.qm` files.
