#!/bin/bash
# Builds build/Клац.app. Needs only the Command Line Tools, no Xcode.
#
#   ./scripts/build-app.sh                 universal app: Apple Silicon + Intel
#   ARCHS=arm64 ./scripts/build-app.sh     this Mac only, faster while developing
#
# SwiftPM can only build several architectures at once with Xcode's build system, so each
# architecture is built on its own and the results are joined with lipo.
set -euo pipefail
cd "$(dirname "$0")/.."

ARCHS="${ARCHS:-arm64 x86_64}"
APP="build/Клац.app"
BUNDLE_ID="io.github.belokoz.klats"
MIN_MACOS="13.0"

binaries=()
for arch in $ARCHS; do
    echo "==> building $arch"
    swift build -c release --triple "${arch}-apple-macosx${MIN_MACOS}"
    binaries+=("$(swift build -c release --triple "${arch}-apple-macosx${MIN_MACOS}" --show-bin-path)/Klats")
done

echo "==> drawing the icon"
mkdir -p build
swiftc -O scripts/make-icon/main.swift Sources/Klats/AppIcon.swift -o build/make-icon
build/make-icon
iconutil -c icns build/AppIcon.iconset -o Resources/AppIcon.icns

echo "==> assembling $APP"
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
lipo -create "${binaries[@]}" -output "$APP/Contents/MacOS/Klats"
cp Resources/Info.plist "$APP/Contents/Info.plist"
cp Resources/AppIcon.icns "$APP/Contents/Resources/AppIcon.icns"
cp -R Resources/en.lproj "$APP/Contents/Resources/"
# A Russian .lproj (even empty) tells macOS the app speaks Russian, so it stays Russian by default.
mkdir -p "$APP/Contents/Resources/ru.lproj"

# Signing. There is no Apple Developer account behind this project, so downloaded copies need
# «Open Anyway» once either way. But the choice of signature decides whether the Accessibility
# permission survives updates: it is remembered by signature, and an ad-hoc signature changes
# with every build. The permanent «Klats Signing» certificate (scripts/make-signing-cert.sh)
# keeps it stable; without that certificate the build falls back to ad-hoc.
SIGN_IDENTITY="${SIGN_IDENTITY:-}"
if [[ -z "$SIGN_IDENTITY" ]] && security find-certificate -c "Klats Signing" >/dev/null 2>&1; then
    SIGN_IDENTITY="Klats Signing"
fi
echo "==> signing with ${SIGN_IDENTITY:-an ad-hoc signature}"
codesign --force --sign "${SIGN_IDENTITY:--}" --identifier "$BUNDLE_ID" "$APP" 2>&1 | grep -v "unable to build chain" || true

echo "==> done"
lipo -info "$APP/Contents/MacOS/Klats"
du -sh "$APP" | awk '{print "size: " $1}'
