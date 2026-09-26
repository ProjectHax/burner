#!/bin/bash
# build-rpm.sh - Build RPM packages for RHEL 10 / Fedora
# This script creates source and binary RPMs

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$(dirname "$SCRIPT_DIR")")"
VERSION="${VERSION:-$(cat "$PROJECT_DIR/VERSION")}"
RELEASE="${RELEASE:-1}"

echo "========================================"
echo "Building Burner RPM v$VERSION-$RELEASE"
echo "========================================"
echo ""

# Check for rpmbuild
if ! command -v rpmbuild &> /dev/null; then
    echo "Error: rpmbuild not found"
    echo "Install with: sudo dnf install rpm-build rpmdevtools"
    exit 1
fi

# Set up RPM build tree
RPMBUILD_DIR="$PROJECT_DIR/build/rpmbuild"
mkdir -p "$RPMBUILD_DIR"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

# Compile translations first (if lrelease is available)
echo "Compiling translations..."
if command -v lrelease-qt6 &> /dev/null; then
    LRELEASE="lrelease-qt6"
elif command -v lrelease6 &> /dev/null; then
    LRELEASE="lrelease6"
elif command -v lrelease &> /dev/null; then
    LRELEASE="lrelease"
else
    echo "Warning: lrelease not found, using pre-compiled translations if available"
    LRELEASE=""
fi

if [ -n "$LRELEASE" ]; then
    for ts in "$PROJECT_DIR/translations/"*.ts; do
        if [ -f "$ts" ]; then
            qm="${ts%.ts}.qm"
            echo "  Compiling $(basename "$ts")..."
            "$LRELEASE" "$ts" -qm "$qm" 2>/dev/null || true
        fi
    done
fi

# Create source tarball
echo "Creating source tarball..."
TARBALL="$RPMBUILD_DIR/SOURCES/burner-$VERSION.tar.gz"

# Create a temporary directory for the tarball contents
TARBALL_TMP=$(mktemp -d)
TARBALL_SRC="$TARBALL_TMP/burner-$VERSION"
mkdir -p "$TARBALL_SRC"

# Copy source files
cd "$PROJECT_DIR"
cp -r CMakeLists.txt cmake src resources translations packaging scripts "$TARBALL_SRC/" 2>/dev/null || true
cp VERSION CLAUDE.md README.md LICENSE* "$TARBALL_SRC/" 2>/dev/null || true

# Include compiled .qm files in translations directory
if ls "$PROJECT_DIR/translations/"*.qm 1>/dev/null 2>&1; then
    cp "$PROJECT_DIR/translations/"*.qm "$TARBALL_SRC/translations/"
fi

# Create the tarball
cd "$TARBALL_TMP"
tar -czf "$TARBALL" "burner-$VERSION"
rm -rf "$TARBALL_TMP"
cd "$PROJECT_DIR"

# Copy spec file
cp "$SCRIPT_DIR/burner.spec" "$RPMBUILD_DIR/SPECS/"

# Update version in spec file
sed -i "s/^Version:.*/Version:        $VERSION/" "$RPMBUILD_DIR/SPECS/burner.spec"
sed -i "s/^Release:.*/Release:        $RELEASE%{?dist}/" "$RPMBUILD_DIR/SPECS/burner.spec"

# Replace the changelog with an entry for this version; release notes live on GitHub
sed -i '/^%changelog/,$d' "$RPMBUILD_DIR/SPECS/burner.spec"
cat >> "$RPMBUILD_DIR/SPECS/burner.spec" << CHANGELOG
%changelog
* $(LC_ALL=C date -u +"%a %b %d %Y") ProjectHax LLC - $VERSION-$RELEASE
- Release $VERSION: https://github.com/ProjectHax/burner/releases
CHANGELOG

echo "Building RPM..."

# Build the RPM
rpmbuild -ba \
    --define "_topdir $RPMBUILD_DIR" \
    "$RPMBUILD_DIR/SPECS/burner.spec"

echo ""
echo "========================================"
echo "RPM build complete!"
echo ""
echo "Source RPM:"
ls -1 "$RPMBUILD_DIR/SRPMS/"*.rpm 2>/dev/null || echo "  (none)"
echo ""
echo "Binary RPMs:"
ls -1 "$RPMBUILD_DIR/RPMS/"*/*.rpm 2>/dev/null || echo "  (none)"
echo "========================================"
