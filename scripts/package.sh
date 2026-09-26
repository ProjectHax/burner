#!/bin/bash
# package.sh - Generate distribution packages after building
# Usage: ./scripts/package.sh [appimage|rpm|all]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${BUILD_DIR:-$PROJECT_DIR/build}"

# Use VERSION from environment, or the VERSION file
VERSION="${VERSION:-$(cat "$PROJECT_DIR/VERSION")}"
export VERSION
echo "Package version: $VERSION"

show_usage() {
    echo "Usage: $0 [command]"
    echo ""
    echo "Commands:"
    echo "  appimage    Build AppImage package"
    echo "  rpm         Build RPM package (RHEL 10 / Fedora)"
    echo "  deb         Build .deb package (Debian/Ubuntu) using fpm"
    echo "  flatpak     Build Flatpak package"
    echo "  all         Build all packages (AppImage, RPM, DEB)"
    echo "  check       Check build prerequisites"
    echo ""
    echo "Environment variables:"
    echo "  BUILD_DIR   Build directory (default: ./build)"
    echo "  VERSION     Package version (default: from the VERSION file)"
    echo ""
    echo "Example:"
    echo "  # Build the application first"
    echo "  mkdir build && cd build && cmake .. && make -j\$(nproc)"
    echo ""
    echo "  # Then create packages"
    echo "  ./scripts/package.sh all"
}

check_build() {
    if [ ! -f "$BUILD_DIR/src/burner" ]; then
        echo "Error: Build not found at $BUILD_DIR/src/burner"
        echo ""
        echo "Build the application first:"
        echo "  mkdir -p build && cd build"
        echo "  cmake .."
        echo "  make -j\$(nproc)"
        return 1
    fi
    echo "Build found: $BUILD_DIR/src/burner"
    return 0
}

check_prerequisites() {
    echo "Checking prerequisites..."
    echo ""

    local missing=0

    # Check build
    if check_build; then
        echo "  [OK] Application binary"
    else
        echo "  [MISSING] Application binary"
        missing=1
    fi

    # Check for RPM tools
    if command -v rpmbuild &> /dev/null; then
        echo "  [OK] rpmbuild"
    else
        echo "  [MISSING] rpmbuild - install with: sudo dnf install rpm-build"
        missing=1
    fi

    # Check for curl (needed to download linuxdeploy)
    if command -v curl &> /dev/null; then
        echo "  [OK] curl"
    else
        echo "  [MISSING] curl - install with: sudo dnf install curl"
        missing=1
    fi

    # Check for FUSE (needed for AppImage)
    if [ -e /dev/fuse ]; then
        echo "  [OK] FUSE"
    else
        echo "  [MISSING] FUSE - install with: sudo dnf install fuse fuse-libs"
        missing=1
    fi

    # Check for fpm (needed for .deb)
    if command -v fpm &> /dev/null; then
        echo "  [OK] fpm ($(fpm --version))"
    else
        echo "  [MISSING] fpm - install with: gem install fpm"
        missing=1
    fi

    # Check for flatpak-builder (needed for Flatpak)
    if command -v flatpak-builder &> /dev/null; then
        echo "  [OK] flatpak-builder"
    else
        echo "  [MISSING] flatpak-builder - install with: sudo dnf install flatpak-builder"
        missing=1
    fi

    echo ""
    if [ $missing -eq 0 ]; then
        echo "All prerequisites satisfied!"
        return 0
    else
        echo "Some prerequisites are missing."
        return 1
    fi
}

build_appimage() {
    echo ""
    echo "========================================"
    echo "Building AppImage"
    echo "========================================"

    check_build || exit 1

    chmod +x "$PROJECT_DIR/packaging/appimage/build-appimage.sh"
    "$PROJECT_DIR/packaging/appimage/build-appimage.sh"
}

build_rpm() {
    echo ""
    echo "========================================"
    echo "Building RPM"
    echo "========================================"

    # rpmbuild compiles from a source tarball, doesn't need pre-built binary
    chmod +x "$PROJECT_DIR/packaging/rpm/build-rpm.sh"
    "$PROJECT_DIR/packaging/rpm/build-rpm.sh"
}

build_deb() {
    echo ""
    echo "========================================"
    echo "Building .deb"
    echo "========================================"

    check_build || exit 1

    chmod +x "$PROJECT_DIR/packaging/deb/build-deb.sh"
    "$PROJECT_DIR/packaging/deb/build-deb.sh"
}

build_flatpak() {
    echo ""
    echo "========================================"
    echo "Building Flatpak"
    echo "========================================"

    # Flatpak builds from source, doesn't need pre-built binary
    chmod +x "$PROJECT_DIR/packaging/flatpak/build-flatpak.sh"
    "$PROJECT_DIR/packaging/flatpak/build-flatpak.sh"
}

build_all() {
    echo "Building all packages for Burner v$VERSION"
    echo ""

    build_appimage
    build_rpm
    build_deb

    echo ""
    echo "========================================"
    echo "All packages built successfully!"
    echo "========================================"
    echo ""
    echo "Packages:"
    ls -1 "$BUILD_DIR/"*.AppImage 2>/dev/null | sed 's/^/  /' || true
    ls -1 "$BUILD_DIR/rpmbuild/RPMS/"*/*.rpm 2>/dev/null | sed 's/^/  /' || true
    ls -1 "$BUILD_DIR/rpmbuild/SRPMS/"*.rpm 2>/dev/null | sed 's/^/  /' || true
    ls -1 "$BUILD_DIR/"*.deb 2>/dev/null | sed 's/^/  /' || true
    ls -1 "$BUILD_DIR/"*.flatpak 2>/dev/null | sed 's/^/  /' || true
}

# Main
case "${1:-}" in
    appimage)
        build_appimage
        ;;
    rpm)
        build_rpm
        ;;
    deb)
        build_deb
        ;;
    flatpak)
        build_flatpak
        ;;
    all)
        build_all
        ;;
    check)
        check_prerequisites
        ;;
    -h|--help|"")
        show_usage
        ;;
    *)
        echo "Unknown command: $1"
        echo ""
        show_usage
        exit 1
        ;;
esac
