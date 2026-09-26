#!/bin/bash
# build-deb.sh - Build Debian/Ubuntu .deb packages using fpm
# Works on any Linux system (RHEL, Fedora, etc.)
#
# Prerequisites:
#   sudo dnf install ruby ruby-devel gcc make rpm-build  # RHEL/Fedora
#   gem install fpm

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$(dirname "$SCRIPT_DIR")")"
BUILD_DIR="${BUILD_DIR:-$PROJECT_DIR/build}"
VERSION="${VERSION:-$(cat "$PROJECT_DIR/VERSION")}"
ARCH="${ARCH:-amd64}"
RELEASE="${RELEASE:-1}"

# Each Debian/Ubuntu release has different library versions, so tag the
# package revision with the release it was built on (e.g. 1~ubuntu22.04).
# Read os-release in a subshell: it defines its own VERSION variable.
if [ -r /etc/os-release ]; then
    OS_ID=$(. /etc/os-release && echo "$ID")
    OS_VERSION_ID=$(. /etc/os-release && echo "$VERSION_ID")
    if [[ "$OS_ID" == ubuntu || "$OS_ID" == debian ]]; then
        RELEASE="$RELEASE~$OS_ID$OS_VERSION_ID"
    fi
fi

echo "========================================"
echo "Building Burner .deb v$VERSION"
echo "========================================"
echo ""

# Check for fpm
if ! command -v fpm &> /dev/null; then
    echo "Error: fpm not found"
    echo ""
    echo "Install fpm on RHEL/Fedora:"
    echo "  sudo dnf install ruby ruby-devel gcc make rpm-build"
    echo "  gem install fpm"
    echo ""
    echo "Or with sudo:"
    echo "  sudo gem install fpm"
    exit 1
fi

# Check build exists
if [ ! -f "$BUILD_DIR/src/burner" ]; then
    echo "Error: Build not found at $BUILD_DIR/src/burner"
    echo "Run 'cmake --build build' first"
    exit 1
fi

# Create staging directory
STAGING="$BUILD_DIR/deb-staging"
rm -rf "$STAGING"
mkdir -p "$STAGING/usr/bin"
mkdir -p "$STAGING/usr/share/applications"
mkdir -p "$STAGING/usr/share/icons/hicolor/scalable/apps"
mkdir -p "$STAGING/usr/share/burner/translations"

echo "Staging files..."

# Copy binary
cp "$BUILD_DIR/src/burner" "$STAGING/usr/bin/"
chmod 755 "$STAGING/usr/bin/burner"

# Copy desktop file
cp "$PROJECT_DIR/packaging/burner.desktop" "$STAGING/usr/share/applications/"

# Copy icon
cp "$PROJECT_DIR/packaging/burner.svg" "$STAGING/usr/share/icons/hicolor/scalable/apps/"

# Copy translations if available
if ls "$PROJECT_DIR/translations/"*.qm 1> /dev/null 2>&1; then
    cp "$PROJECT_DIR/translations/"*.qm "$STAGING/usr/share/burner/translations/"
fi

DEPENDS=()
if command -v dpkg-shlibdeps &> /dev/null; then
    # On Debian/Ubuntu, derive dependencies from the libraries the binary links
    echo "Resolving dependencies with dpkg-shlibdeps..."
    SHLIBDEPS_DIR=$(mktemp -d)
    mkdir -p "$SHLIBDEPS_DIR/debian"
    printf 'Source: burner\n\nPackage: burner\nArchitecture: any\n' > "$SHLIBDEPS_DIR/debian/control"
    SHLIBS=$(cd "$SHLIBDEPS_DIR" && dpkg-shlibdeps -O "$STAGING/usr/bin/burner")
    rm -rf "$SHLIBDEPS_DIR"

    IFS=',' read -ra LIBS <<< "${SHLIBS#shlibs:Depends=}"
    for dep in "${LIBS[@]}"; do
        dep="${dep#"${dep%%[![:space:]]*}"}"
        DEPENDS+=(--depends "$dep")
    done
else
    # Cross-building (e.g. on RHEL): fall back to a static list
    DEPENDS=(
        --depends "libqt6core6 | qt6-base-abi-6"
        --depends "libqt6gui6 | qt6-base-abi-6"
        --depends "libqt6widgets6 | qt6-base-abi-6"
        --depends "libburn4"
        --depends "libisofs6"
        --depends "libisoburn1"
        --depends "libavcodec59 | libavcodec60 | libavcodec-extra"
        --depends "libavformat59 | libavformat60 | libavformat-extra"
        --depends "libswresample4"
        --depends "libswscale6 | libswscale7"
        --depends "libcurl4 | libcurl4t64"
    )
fi

# Qt's platform plugins (xcb, ...) are loaded at runtime, so dpkg-shlibdeps
# can't see them, and on Ubuntu 22.04 nothing else pulls them in
DEPENDS+=(--depends "qt6-qpa-plugins")

echo "Building .deb with fpm..."

cd "$BUILD_DIR"

# Build the .deb package
fpm -s dir -t deb \
    --name burner \
    --version "$VERSION" \
    --iteration "$RELEASE" \
    --architecture "$ARCH" \
    --description "CD/DVD/Blu-ray Burning Application" \
    --url "https://github.com/projecthax/burner" \
    --maintainer "ProjectHax LLC" \
    --license "GPL-3.0-or-later" \
    --category "video" \
    "${DEPENDS[@]}" \
    --deb-recommends "qt6-wayland" \
    --deb-priority optional \
    --deb-compression xz \
    --force \
    -C "$STAGING" \
    usr

# Clean up staging
rm -rf "$STAGING"

echo ""
echo "========================================"
echo ".deb package created successfully!"
echo "Output: $BUILD_DIR/burner_${VERSION}-${RELEASE}_${ARCH}.deb"
echo "========================================"
echo ""
echo "Install on Debian/Ubuntu with:"
echo "  sudo apt install ./burner_${VERSION}-${RELEASE}_${ARCH}.deb"
