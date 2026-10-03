#pragma once
#include <cstdint>
#include "envmon_ioctl.h"

namespace envmon {

/// Engineering-unit view of a raw kernel sample.
struct Reading {
    uint32_t seq{0};
    uint64_t timestamp_ns{0};
    double   temp_c{0.0};
    double   humidity_pct{0.0};
    uint32_t flags{0};
};

inline Reading toReading(const envmon_sample& s) {
    return Reading{s.seq, s.timestamp_ns, s.temp_mC / 1000.0,
                   s.humidity_mpct / 1000.0, s.flags};
}

}  // namespace envmon
