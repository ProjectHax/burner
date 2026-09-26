#!/bin/bash
# build-appimage.sh - Build an AppImage for Burner
# Prerequisites: linuxdeploy, linuxdeploy-plugin-qt

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$(dirname "$SCRIPT_DIR")")"
BUILD_DIR="${BUILD_DIR:-$PROJECT_DIR/build}"
APPDIR="$BUILD_DIR/AppDir"
VERSION="${VERSION:-$(cat "$PROJECT_DIR/VERSION")}"

echo "========================================"
echo "Building Burner AppImage v$VERSION"
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

# Download linuxdeploy if not present
LINUXDEPLOY="$BUILD_DIR/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT="$BUILD_DIR/linuxdeploy-plugin-qt-x86_64.AppImage"

if [ ! -f "$LINUXDEPLOY" ]; then
    echo "Downloading linuxdeploy..."
    curl -L -o "$LINUXDEPLOY" \
        "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
    chmod +x "$LINUXDEPLOY"
fi

if [ ! -f "$LINUXDEPLOY_QT" ]; then
    echo "Downloading linuxdeploy-plugin-qt..."
    curl -L -o "$LINUXDEPLOY_QT" \
        "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
    chmod +x "$LINUXDEPLOY_QT"
fi

# Ensure build exists
if [ ! -f "$BUILD_DIR/src/burner" ]; then
    echo "Error: Build not found at $BUILD_DIR/src/burner"
    echo "Run 'cmake --build build' first"
    exit 1
fi

# Clean previous AppDir
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/scalable/apps"
mkdir -p "$APPDIR/usr/share/burner/translations"

echo "Copying files to AppDir..."

# Copy binary
cp "$BUILD_DIR/src/burner" "$APPDIR/usr/bin/"

# Copy desktop file
cp "$PROJECT_DIR/packaging/burner.desktop" "$APPDIR/usr/share/applications/"

# Copy icon
cp "$PROJECT_DIR/packaging/burner.svg" "$APPDIR/usr/share/icons/hicolor/scalable/apps/"

# Copy translations if compiled
if ls "$PROJECT_DIR/translations/"*.qm 1> /dev/null 2>&1; then
    cp "$PROJECT_DIR/translations/"*.qm "$APPDIR/usr/share/burner/translations/"
fi

echo "Running linuxdeploy..."

# Configure Qt paths for linuxdeploy-plugin-qt
# Priority: 1) Environment override, 2) System Qt6 from distro packages
if [ -z "$QMAKE" ]; then
    # Try to find system qmake6 (RHEL/Fedora package: qt6-qtbase-devel)
    if command -v qmake6 &> /dev/null; then
        export QMAKE="$(command -v qmake6)"
        echo "Using system Qt6: $QMAKE"
    elif command -v qmake-qt6 &> /dev/null; then
        export QMAKE="$(command -v qmake-qt6)"
        echo "Using system Qt6: $QMAKE"
    elif [ -x "/usr/lib64/qt6/bin/qmake" ]; then
        export QMAKE="/usr/lib64/qt6/bin/qmake"
        echo "Using system Qt6: $QMAKE"
    elif [ -x "/usr/bin/qmake6" ]; then
        export QMAKE="/usr/bin/qmake6"
        echo "Using system Qt6: $QMAKE"
    else
        echo "Warning: Could not find system qmake6, linuxdeploy will auto-detect Qt"
    fi
fi

# Set QT_SELECT to prefer Qt6 if qtchooser is installed
export QT_SELECT=qt6

# Verify we're not using a static Qt build
if [ -n "$QMAKE" ]; then
    QT_VERSION=$("$QMAKE" -query QT_VERSION 2>/dev/null || echo "unknown")
    QT_INSTALL_PREFIX=$("$QMAKE" -query QT_INSTALL_PREFIX 2>/dev/null || echo "unknown")
    QT_INSTALL_PLUGINS=$("$QMAKE" -query QT_INSTALL_PLUGINS 2>/dev/null || echo "unknown")
    echo "Qt version: $QT_VERSION"
    echo "Qt prefix: $QT_INSTALL_PREFIX"

    # Check for Wayland platform plugin
    if [ -f "$QT_INSTALL_PLUGINS/platforms/libqwayland-generic.so" ] || \
       [ -f "$QT_INSTALL_PLUGINS/platforms/libqwayland-egl.so" ]; then
        echo "Qt Wayland plugin: found"
    else
        echo ""
        echo "WARNING: Qt Wayland plugin not found!"
        echo "Install with: sudo dnf install qt6-qtwayland"
        echo "The AppImage will only support X11/XCB without it."
        echo ""
    fi

    # Warn if using a non-system Qt
    if [[ "$QT_INSTALL_PREFIX" == /home/* ]] || [[ "$QT_INSTALL_PREFIX" == /opt/* ]]; then
        echo ""
        echo "WARNING: Using non-system Qt from $QT_INSTALL_PREFIX"
        echo "If this is a static build, the AppImage may not work correctly."
        echo "Set QMAKE=/usr/bin/qmake6 to use system Qt instead."
        echo ""
        read -p "Continue anyway? [y/N] " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            exit 1
        fi
    fi
fi

cd "$BUILD_DIR"

# Get Qt plugin directory
if [ -n "$QMAKE" ]; then
    QT_PLUGIN_DIR=$("$QMAKE" -query QT_INSTALL_PLUGINS 2>/dev/null)
    QT_LIB_DIR=$("$QMAKE" -query QT_INSTALL_LIBS 2>/dev/null)
else
    QT_PLUGIN_DIR="/usr/lib64/qt6/plugins"
    QT_LIB_DIR="/usr/lib64"
fi

# Step 1: Run linuxdeploy to set up AppDir (without creating AppImage yet)
echo "Setting up AppDir with linuxdeploy..."
NO_STRIP=1 \
"$LINUXDEPLOY" \
    --appdir "$APPDIR" \
    --executable "$APPDIR/usr/bin/burner" \
    --desktop-file "$APPDIR/usr/share/applications/burner.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/scalable/apps/burner.svg" \
    --plugin qt

# Step 2: Manually add Wayland plugins (linuxdeploy-plugin-qt doesn't handle them well)
echo ""
echo "Adding Wayland support..."

# Copy Wayland platform plugins
mkdir -p "$APPDIR/usr/plugins/platforms"
if [ -d "$QT_PLUGIN_DIR/platforms" ]; then
    for plugin in "$QT_PLUGIN_DIR/platforms/libqwayland"*.so; do
        if [ -f "$plugin" ]; then
            echo "  Copying $(basename "$plugin")"
            cp "$plugin" "$APPDIR/usr/plugins/platforms/"
        fi
    done
fi

# Copy Wayland shell integration plugins
if [ -d "$QT_PLUGIN_DIR/wayland-shell-integration" ]; then
    mkdir -p "$APPDIR/usr/plugins/wayland-shell-integration"
    cp "$QT_PLUGIN_DIR/wayland-shell-integration/"*.so "$APPDIR/usr/plugins/wayland-shell-integration/" 2>/dev/null || true
    echo "  Copied wayland-shell-integration plugins"
fi

# Copy Wayland graphics integration plugins
if [ -d "$QT_PLUGIN_DIR/wayland-graphics-integration-client" ]; then
    mkdir -p "$APPDIR/usr/plugins/wayland-graphics-integration-client"
    cp "$QT_PLUGIN_DIR/wayland-graphics-integration-client/"*.so "$APPDIR/usr/plugins/wayland-graphics-integration-client/" 2>/dev/null || true
    echo "  Copied wayland-graphics-integration-client plugins"
fi

# Copy Wayland decoration plugins
if [ -d "$QT_PLUGIN_DIR/wayland-decoration-client" ]; then
    mkdir -p "$APPDIR/usr/plugins/wayland-decoration-client"
    cp "$QT_PLUGIN_DIR/wayland-decoration-client/"*.so "$APPDIR/usr/plugins/wayland-decoration-client/" 2>/dev/null || true
    echo "  Copied wayland-decoration-client plugins"
fi

# Copy required Qt Wayland libraries
echo "  Copying Qt Wayland libraries..."
mkdir -p "$APPDIR/usr/lib"
for lib in libQt6WaylandClient.so* libQt6WaylandEglClientHwIntegration.so* libQt6WlShellIntegration.so*; do
    for libpath in "$QT_LIB_DIR"/$lib; do
        if [ -f "$libpath" ]; then
            cp -L "$libpath" "$APPDIR/usr/lib/" 2>/dev/null || true
            echo "    $(basename "$libpath")"
        fi
    done
done

echo ""
echo "Bundled platform plugins:"
ls -1 "$APPDIR/usr/plugins/platforms/" 2>/dev/null || echo "  (none found)"

# Step 3: Create the AppImage
echo ""
echo "Creating AppImage..."
OUTPUT="Burner-$VERSION-x86_64.AppImage" \
LDAI_OUTPUT="Burner-$VERSION-x86_64.AppImage" \
NO_STRIP=1 \
"$LINUXDEPLOY" \
    --appdir "$APPDIR" \
    --output appimage

echo ""
echo "========================================"
echo "AppImage created successfully!"
echo "Output: $BUILD_DIR/Burner-$VERSION-x86_64.AppImage"
echo "========================================"
