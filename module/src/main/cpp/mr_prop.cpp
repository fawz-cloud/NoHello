// Isolated translation unit for the property scrubber. Uses topjohnwu's vendored
// system_properties (redefine_extname'd to *2 symbols) so we get the real
// prop_info internals and __system_property_update without clashing with bionic.
#include <cstring>
#include <atomic>

#include <api/system_properties.h>
#include <api/_system_properties.h>
#include <system_properties/prop_info.h>

#include <android/log.h>
#include "log.h"

// Ported from snake-4/Zygisk-Assistant (doMrProp). resetprop leaves a forensic
// trace on a property: a non-zero low serial and/or leftover bytes past the
// value's null terminator. Detectors read those to tell a prop was tampered.
// We rewrite the value cleanly (same value, fresh serial) to erase the trace.

static size_t safeStringCopy(char *dest, const char *src, size_t dest_size) {
    if (dest_size < 1)
        return 0;
    *dest = 0;
    strncat(dest, src, dest_size - 1);
    return strlen(dest);
}

static bool shouldResetProperty(const prop_info *pi) {
    // Only read-only props with short values are candidates.
    if (strncmp(pi->name, "ro.", 3) != 0 || pi->is_long())
        return false;

    // A modified serial (non-zero low 24 bits) marks a resetprop'd value.
    auto serial = std::atomic_load_explicit(&pi->serial, std::memory_order_relaxed);
    if ((serial & 0xFFFFFF) != 0)
        return true;

    // Leftover bytes past the null terminator also betray a rewrite.
    size_t length = strnlen(pi->value, PROP_VALUE_MAX);
    for (size_t i = length; i < PROP_VALUE_MAX; i++) {
        if (pi->value[i] != '\0')
            return true;
    }

    return false;
}

void doMrProp() {
    static bool isInitialized = false;
    static int resetCount = 0;
    if (!isInitialized) {
        if (__system_properties_init() == -1) {
            LOGE("#[doMrProp] Could not initialize system_properties");
            return;
        }
        isInitialized = true;
    }

    __system_property_foreach(
        [](const prop_info *pi, void *) {
            if (shouldResetProperty(pi)) {
                // strncpy with overlapping pointers is UB, so copy first.
                char buffer[PROP_VALUE_MAX];
                size_t length = safeStringCopy(buffer, pi->value, PROP_VALUE_MAX);
                __system_property_update(const_cast<prop_info *>(pi), buffer, length);
                resetCount++;
            }
        },
        nullptr);

    LOGD("#[doMrProp] resetCount=%d", resetCount);
}
