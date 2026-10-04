#pragma once
#include <cstddef>
#include <deque>

namespace envmon {

/// Running statistics (Welford's algorithm) plus a sliding-window average.
class Statistics {
public:
    explicit Statistics(std::size_t window = 20);
    void   add(double v);
    void   reset();
    std::size_t count() const { return n_; }
    double min() const { return n_ ? min_ : 0.0; }
    double max() const { return n_ ? max_ : 0.0; }
    double mean() const { return n_ ? mean_ : 0.0; }
    double stddev() const;                 // population std-dev
    double movingAverage() const;          // over the last `window` values

private:
    std::size_t window_;
    std::size_t n_{0};
    double mean_{0}, m2_{0}, min_{0}, max_{0}, windowSum_{0};
    std::deque<double> win_;
};

} 
