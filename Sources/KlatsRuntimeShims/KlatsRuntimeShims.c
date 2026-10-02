// Swift 6 compilers call `_stdlib_isOSVersionAtLeastOrVariantVersionAtLeast` from SwiftUI and
// standard library code they inline into the app (SwiftUI's `tag(_:)`, arrays of Objective-C
// objects), but that runtime function only exists from macOS 15: it was added to Swift in
// August 2024. The SDK lists it with no availability, so the build links cleanly against the
// system library, and on macOS 12–14 the system then refused to start Klats at all
// («Не удается открыть программу»). Defined here, the linker binds those calls to this copy
// inside the app, and older systems have nothing to look up.
//
// The «variant» version is the iOS version of a Mac Catalyst app, which Klats is not, so only
// the macOS version counts: that is what the runtime's own implementation answers for a macOS
// process. It forwards to `_stdlib_isOSVersionAtLeast`, which every Swift runtime has.
//
// Both are Swift functions taking machine words and returning a one-bit Bool; on x86_64 and
// arm64 that is the same as the C calling convention used here.

#include <stdbool.h>
#include <stdint.h>

extern bool klats_stdlib_isOSVersionAtLeast(intptr_t major, intptr_t minor, intptr_t patch)
    __asm__("_$ss26_stdlib_isOSVersionAtLeastyBi1_Bw_BwBwtF");

bool klats_stdlib_isOSVersionAtLeastOrVariantVersionAtLeast(
    intptr_t major, intptr_t minor, intptr_t patch,
    intptr_t variantMajor, intptr_t variantMinor, intptr_t variantPatch)
    __asm__("_$ss042_stdlib_isOSVersionAtLeastOrVariantVersiondE0yBi1_Bw_BwBwBwBwBwtF");

bool klats_stdlib_isOSVersionAtLeastOrVariantVersionAtLeast(
    intptr_t major, intptr_t minor, intptr_t patch,
    intptr_t variantMajor, intptr_t variantMinor, intptr_t variantPatch) {
    (void)variantMajor;
    (void)variantMinor;
    (void)variantPatch;
    return klats_stdlib_isOSVersionAtLeast(major, minor, patch);
}
