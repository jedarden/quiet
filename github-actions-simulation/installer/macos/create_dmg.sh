#!/bin/bash

# QUIET macOS DMG Creation Script

set -e

echo "Creating QUIET DMG installer..."

# Variables
APP_NAME="QUIET"
VERSION="1.0.0"
DMG_NAME="${APP_NAME}.dmg"
VOLUME_NAME="${APP_NAME} ${VERSION}"
BUILD_DIR="build"
APP_PATH="${BUILD_DIR}/${APP_NAME}.app"

# Check if app bundle exists
if [ ! -d "${APP_PATH}" ]; then
    echo "Error: ${APP_PATH} not found. Please build the application first."
    exit 1
fi

# Create temporary directory for DMG contents
TEMP_DIR=$(mktemp -d)
trap "rm -rf ${TEMP_DIR}" EXIT

# Copy app bundle to temp directory
echo "Copying application bundle..."
cp -R "${APP_PATH}" "${TEMP_DIR}/"

# Create Applications symlink
ln -s /Applications "${TEMP_DIR}/Applications"

# Copy additional files
cp LICENSE "${TEMP_DIR}/"
cp installer/macos/README.md "${TEMP_DIR}/README.md"

# Create DMG
echo "Creating DMG..."
hdiutil create -volname "${VOLUME_NAME}" \
    -srcfolder "${TEMP_DIR}" \
    -ov -format UDZO \
    "${DMG_NAME}"

# Sign DMG if certificate is available
if [ -n "$DEVELOPER_ID" ]; then
    echo "Signing DMG..."
    codesign --force --sign "$DEVELOPER_ID" "${DMG_NAME}"
fi

echo "DMG created successfully: ${DMG_NAME}"

# Verify DMG
echo "Verifying DMG..."
hdiutil verify "${DMG_NAME}"

echo "Done!"