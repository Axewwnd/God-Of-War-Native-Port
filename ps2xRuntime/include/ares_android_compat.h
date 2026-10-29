#pragma once

#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <locale.h>

namespace ares
{
// NDK libc++ r28 has no floating-point from_chars. Use a private C locale,
// consume the entire decimal and retain the original finite/range checks.
inline bool parseFiniteDecimal(const char *text, double &value)
{
    if (!text || !*text || *text == '+')
        return false;
    const char *p = text;
    if (*p == '-') ++p;
    bool digits = false;
    while (*p >= '0' && *p <= '9') { digits = true; ++p; }
    if (*p == '.') {
        ++p;
        while (*p >= '0' && *p <= '9') { digits = true; ++p; }
    }
    if (!digits) return false;
    if (*p == 'e' || *p == 'E') {
        ++p;
        if (*p == '+' || *p == '-') ++p;
        const char *exponent = p;
        while (*p >= '0' && *p <= '9') ++p;
        if (p == exponent) return false;
    }
    if (*p) return false;
    static locale_t cLocale = newlocale(LC_NUMERIC_MASK, "C", nullptr);
    if (!cLocale) return false;
    char *end = nullptr;
    errno = 0;
    const double parsed = strtod_l(text, &end, cLocale);
    if (errno == ERANGE || end == text || *end || !std::isfinite(parsed))
        return false;
    value = parsed;
    return true;
}

// Bulk countdown skipping is an optional optimization from a newer scheduler.
// On the runtime base, skip zero iterations: the actual recompiled countdown
// still executes every instruction/checkpoint, advancing timers and IOP normally.
template <class Scheduler>
uint64_t advanceQuietRepeated(Scheduler &scheduler, uint32_t cycles, uint64_t count)
{
    if constexpr (requires { scheduler.advanceQuietRepeated(cycles, count); })
        return scheduler.advanceQuietRepeated(cycles, count);
    else
        return 0;
}
}
