#!/bin/bash
# Builds the universal app and packs it into build/Клац-<version>.dmg for a GitHub release.
set -euo pipefail
cd "$(dirname "$0")/.."

VERSION="$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" Resources/Info.plist)"
./scripts/build-app.sh

STAGE="build/dmg"
rm -rf "$STAGE"
mkdir -p "$STAGE"
cp -R "build/Клац.app" "$STAGE/"
ln -s /Applications "$STAGE/Applications"

# GitHub strips non-ASCII characters from release asset names, so the file is Latin; the volume inside is «Клац».
DMG="build/Klats-$VERSION.dmg"
rm -f "$DMG"
hdiutil create -volname "Клац" -srcfolder "$STAGE" -ov -format UDZO "$DMG" >/dev/null
echo "==> $DMG"
shasum -a 256 "$DMG"
