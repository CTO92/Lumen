#!/bin/bash
# ============================================================================
# Lumen AppImage Build Script
# Target: Ubuntu 20.04+
# ============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_DIR}/build-appimage"
APPDIR="${BUILD_DIR}/AppDir"

echo "=== Lumen AppImage Builder ==="
echo "Project directory: ${PROJECT_DIR}"
echo "Build directory: ${BUILD_DIR}"

# Check for required tools
check_tool() {
    if ! command -v "$1" &> /dev/null; then
        echo "Error: $1 is required but not installed."
        echo "Install with: $2"
        exit 1
    fi
}

check_tool cmake "apt install cmake"
check_tool qmake6 "apt install qt6-base-dev"

# Download linuxdeploy if not present
LINUXDEPLOY="${BUILD_DIR}/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT="${BUILD_DIR}/linuxdeploy-plugin-qt-x86_64.AppImage"

if [ ! -f "$LINUXDEPLOY" ]; then
    echo "Downloading linuxdeploy..."
    wget -q -O "$LINUXDEPLOY" \
        "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
    chmod +x "$LINUXDEPLOY"
fi

if [ ! -f "$LINUXDEPLOY_QT" ]; then
    echo "Downloading linuxdeploy Qt plugin..."
    wget -q -O "$LINUXDEPLOY_QT" \
        "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
    chmod +x "$LINUXDEPLOY_QT"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo "Configuring build..."
cmake "$PROJECT_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DBUILD_GUI=ON \
    -DBUILD_CLI=OFF \
    -DBUILD_SERVER=OFF \
    -DBUILD_TESTS=OFF

# Build
echo "Building..."
cmake --build . --parallel $(nproc)

# Install to AppDir
echo "Installing to AppDir..."
DESTDIR="$APPDIR" cmake --install .

# Create desktop file
mkdir -p "${APPDIR}/usr/share/applications"
cat > "${APPDIR}/usr/share/applications/lumen.desktop" << 'EOF'
[Desktop Entry]
Type=Application
Name=Lumen
GenericName=Portfolio Optimizer
Comment=Quantum-Enhanced Portfolio Optimization
Exec=lumen-gui %F
Icon=lumen
Terminal=false
Categories=Office;Finance;
Keywords=portfolio;optimization;quantum;investment;
MimeType=application/x-lumen-portfolio;
StartupNotify=true
EOF

# Create icon directories and copy icon
mkdir -p "${APPDIR}/usr/share/icons/hicolor/256x256/apps"
if [ -f "${PROJECT_DIR}/apps/gui/resources/icons/lumen.png" ]; then
    cp "${PROJECT_DIR}/apps/gui/resources/icons/lumen.png" \
       "${APPDIR}/usr/share/icons/hicolor/256x256/apps/"
else
    # Create a simple placeholder icon
    echo "Warning: Icon not found, creating placeholder..."
    convert -size 256x256 xc:navy \
            -fill white -gravity center -pointsize 72 -annotate 0 "L" \
            "${APPDIR}/usr/share/icons/hicolor/256x256/apps/lumen.png" 2>/dev/null || \
    python3 -c "
from PIL import Image, ImageDraw
img = Image.new('RGB', (256, 256), color='#0078D4')
draw = ImageDraw.Draw(img)
draw.text((90, 60), 'L', fill='white')
img.save('${APPDIR}/usr/share/icons/hicolor/256x256/apps/lumen.png')
" 2>/dev/null || echo "Could not create icon"
fi

# Build AppImage
echo "Building AppImage..."
export QMAKE=qmake6
export VERSION="1.0.0"
export OUTPUT="${BUILD_DIR}/Lumen-${VERSION}-x86_64.AppImage"

"$LINUXDEPLOY" \
    --appdir "$APPDIR" \
    --desktop-file "${APPDIR}/usr/share/applications/lumen.desktop" \
    --icon-file "${APPDIR}/usr/share/icons/hicolor/256x256/apps/lumen.png" \
    --plugin qt \
    --output appimage

echo "=== Build Complete ==="
echo "AppImage: $OUTPUT"
echo ""
echo "To run: chmod +x '$OUTPUT' && '$OUTPUT'"
