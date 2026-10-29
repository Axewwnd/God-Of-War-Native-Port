#include "ares_android_compat.h"
#include <cassert>
#include <initializer_list>

struct BaseScheduler { unsigned calls = 0; };
struct BulkScheduler {
    unsigned calls = 0;
    uint64_t advanceQuietRepeated(uint32_t cycles, uint64_t count) {
        ++calls;
        assert(cycles == 7);
        return count;
    }
};

int main()
{
    double value = 42;
    assert(ares::parseFiniteDecimal("0", value) && value == 0);
    assert(ares::parseFiniteDecimal(".5", value) && value == .5);
    assert(ares::parseFiniteDecimal("-1.25e+2", value) && value == -125);
    for (const char *invalid : {"", " ", " 1", "1 ", "+1", "0x1", "nan", "inf", "1e", "1x", "1e9999", "1e-9999"}) {
        value = 42;
        assert(!ares::parseFiniteDecimal(invalid, value));
        assert(value == 42);
    }
    BaseScheduler base;
    assert(ares::advanceQuietRepeated(base, 7, 100) == 0);
    assert(base.calls == 0); // no iterations skipped or cycles silently consumed
    BulkScheduler bulk;
    assert(ares::advanceQuietRepeated(bulk, 7, 100) == 100);
    assert(bulk.calls == 1);
}
