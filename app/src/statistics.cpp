#include "envmon/statistics.hpp"
#include <algorithm>
#include <cmath>

namespace envmon {

Statistics::Statistics(std::size_t window) : window_(window ? window : 1) {}

void Statistics::add(double v) {
    ++n_;
    if (n_ == 1) { min_ = max_ = v; }
    else { min_ = std::min(min_, v); max_ = std::max(max_, v); }
    double delta = v - mean_;          // Welford
    mean_ += delta / static_cast<double>(n_);
    m2_ += delta * (v - mean_);

    win_.push_back(v);
    windowSum_ += v;
    if (win_.size() > window_) { windowSum_ -= win_.front(); win_.pop_front(); }
}

void Statistics::reset() {
    n_ = 0; mean_ = m2_ = min_ = max_ = windowSum_ = 0;
    win_.clear();
}

double Statistics::stddev() const {
    return n_ > 1 ? std::sqrt(m2_ / static_cast<double>(n_)) : 0.0;
}

double Statistics::movingAverage() const {
    return win_.empty() ? 0.0 : windowSum_ / static_cast<double>(win_.size());
}

}  // namespace envmon
