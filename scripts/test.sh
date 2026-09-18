#!/bin/bash
# Runs the unit tests. With full Xcode a plain `swift test` is enough. With only the
# Command Line Tools installed, SwiftPM cannot find the Swift Testing framework on its
# own, so this script points it at the copy that ships inside the tools.
set -euo pipefail
cd "$(dirname "$0")/.."

DEV_DIR="$(xcode-select -p)"
if [[ "$DEV_DIR" == *CommandLineTools* ]]; then
    FW="$DEV_DIR/Library/Developer/Frameworks"
    LIB="$DEV_DIR/Library/Developer/usr/lib"
    PLUGINS="$DEV_DIR/usr/lib/swift/host/plugins/testing"
    exec swift test \
        -Xswiftc -F -Xswiftc "$FW" \
        -Xswiftc -plugin-path -Xswiftc "$PLUGINS" \
        -Xlinker -F -Xlinker "$FW" \
        -Xlinker -rpath -Xlinker "$FW" \
        -Xlinker -rpath -Xlinker "$LIB" \
        "$@"
else
    exec swift test "$@"
fi
