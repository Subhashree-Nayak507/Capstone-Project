#include "envmon/alert_engine.hpp"
#include <algorithm>

namespace envmon {

const char* toString(AlertState s) {
    switch (s) {
        case AlertState::Normal:   return "NORMAL";
        case AlertState::Warning:  return "WARNING";
        case AlertState::Critical: return "CRITICAL";
    }
    return "?";
}

AlertState AlertEngine::classify(double v, double warn, double crit, double h,
                                 AlertState cur) {
    if (v >= crit) return AlertState::Critical;
    if (v >= warn) {
        if (cur == AlertState::Critical && v >= crit - h) return AlertState::Critical;
        return AlertState::Warning;
    }
    // below the warning limit
    if (cur == AlertState::Critical) {
        if (v >= crit - h) return AlertState::Critical;
        return v < warn - h ? AlertState::Normal : AlertState::Warning;
    }
    if (cur == AlertState::Warning && v >= warn - h) return AlertState::Warning;
    return AlertState::Normal;
}

std::optional<Transition> AlertEngine::update(const Reading& r) {
    temp_ = classify(r.temp_c, th_.warn_temp, th_.crit_temp, th_.hysteresis, temp_);
    hum_  = classify(r.humidity_pct, th_.warn_hum, th_.crit_hum, th_.hysteresis, hum_);
    AlertState next = std::max(temp_, hum_);
    if (next == overall_) return std::nullopt;
    Transition t{overall_, next, r};
    overall_ = next;
    return t;
}

}  // namespace envmon
