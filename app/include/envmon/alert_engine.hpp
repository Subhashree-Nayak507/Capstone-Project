#pragma once
#include <optional>
#include "envmon/types.hpp"

namespace envmon {

enum class AlertState { Normal = 0, Warning = 1, Critical = 2 };
const char* toString(AlertState s);

struct Thresholds {
    double warn_temp{30.0}, crit_temp{35.0};
    double warn_hum{70.0},  crit_hum{85.0};
    double hysteresis{1.0};   // must fall this far below a limit to de-escalate
};

struct Transition {
    AlertState from, to;
    Reading    reading;
};

/// Hysteresis-based alert state machine (see docs/uml/state_machine.md).
/// Temperature and humidity are tracked independently; the overall state is
/// the worse of the two.
class AlertEngine {
public:
    explicit AlertEngine(Thresholds t = {}) : th_(t) {}
    /// Feed one reading; returns a Transition if the overall state changed.
    std::optional<Transition> update(const Reading& r);
    AlertState state() const { return overall_; }
    static AlertState classify(double v, double warn, double crit, double hyst,
                               AlertState current);

private:
    Thresholds th_;
    AlertState temp_{AlertState::Normal}, hum_{AlertState::Normal};
    AlertState overall_{AlertState::Normal};
};

}  // namespace envmon
