#!/bin/bash
# Builds Клац and installs it into /Applications, replacing the running copy.
# Use this during dogfooding: one command from source change to the updated app in the menu bar.
set -euo pipefail
cd "$(dirname "$0")/.."

./scripts/build-app.sh

TARGET="/Applications/Клац.app"
pkill -f '/Клац.app/Contents/MacOS/Klats' 2>/dev/null || true
sleep 1
rm -rf "$TARGET"
cp -R "build/Клац.app" "$TARGET"
open "$TARGET"
echo "==> installed and launched: $TARGET"
