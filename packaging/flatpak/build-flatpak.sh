#!/bin/bash
# build-flatpak.sh - Build a Flatpak for Burner
# Prerequisites: flatpak, flatpak-builder

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$(dirname "$SCRIPT_DIR")")"
BUILD_DIR="${BUILD_DIR:-$PROJECT_DIR/build}"
VERSION="${VERSION:-$(cat "$PROJECT_DIR/VERSION")}"

APP_ID="com.github.projecthax.Burner"
MANIFEST="$SCRIPT_DIR/$APP_ID.yml"

echo "========================================"
echo "Building Burner Flatpak v$VERSION"
echo "========================================"
echo ""

# Check for required tools
check_tool() {
    if ! command -v "$1" &> /dev/null; then
        echo "Error: $1 not found"
        echo "Install with: $2"
        exit 1
    fi
}

check_tool flatpak "sudo dnf install flatpak"
check_tool flatpak-builder "sudo dnf install flatpak-builder"

# Make sure Flathub is available for the KDE runtime and SDK
flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo

# Create build directory
FLATPAK_BUILD_DIR="$BUILD_DIR/flatpak-build"
FLATPAK_REPO="$BUILD_DIR/flatpak-repo"

mkdir -p "$BUILD_DIR"

echo ""
echo "Building Flatpak..."
echo "  Manifest: $MANIFEST"
echo "  Build dir: $FLATPAK_BUILD_DIR"
echo "  Repo: $FLATPAK_REPO"
echo ""

# Build the Flatpak
# --install-deps-from installs the runtime and SDK named in the manifest
flatpak-builder \
    --user \
    --install-deps-from=flathub \
    --force-clean \
    --repo="$FLATPAK_REPO" \
    "$FLATPAK_BUILD_DIR" \
    "$MANIFEST"

echo ""
echo "Creating Flatpak bundle..."

# Create a bundle file for distribution
BUNDLE_FILE="$BUILD_DIR/Burner-$VERSION.flatpak"
flatpak build-bundle \
    "$FLATPAK_REPO" \
    "$BUNDLE_FILE" \
    "$APP_ID"

echo ""
echo "========================================"
echo "Flatpak build complete!"
echo "========================================"
echo ""
echo "Output: $BUNDLE_FILE"
echo ""
echo "To install locally:"
echo "  flatpak install --user $BUNDLE_FILE"
echo ""
echo "To install from the local repo:"
echo "  flatpak remote-add --user --no-gpg-verify burner-repo $FLATPAK_REPO"
echo "  flatpak install --user burner-repo $APP_ID"
echo ""
echo "To run:"
echo "  flatpak run $APP_ID"
