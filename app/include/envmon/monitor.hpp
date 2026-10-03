#pragma once
#include <atomic>
#include <chrono>
#include <fstream>
#include <memory>
#include <mutex>
#include <thread>
#include "envmon/alert_engine.hpp"
#include "envmon/bounded_queue.hpp"
#include "envmon/config.hpp"
#include "envmon/device.hpp"
#include "envmon/statistics.hpp"

namespace envmon {

struct Snapshot {
    uint64_t processed{0};
    uint64_t queue_dropped{0};
    envmon_stats dev{};
    bool     has_last{false};
    Reading  last;
    AlertState state{AlertState::Normal};
    uint64_t alert_transitions{0};
    double   temp_min{0}, temp_max{0}, temp_mean{0}, temp_avg_window{0};
    double   hum_min{0},  hum_max{0},  hum_mean{0};
    double   uptime_s{0};
};

/// Orchestrates three threads:
///   acquisition: IDevice -> BoundedQueue
///   processing : queue -> statistics, alert engine, CSV
///   server     : Unix-socket command interface (STATUS, LAST, ...)
class Monitor {
public:
    Monitor(Config cfg, std::unique_ptr<IDevice> dev);
    ~Monitor();
    Monitor(const Monitor&) = delete;
    Monitor& operator=(const Monitor&) = delete;

    bool start(std::string& err);
    void requestStop();
    void join();
    bool running() const { return !stop_.load(); }
    Snapshot snapshot();

private:
    void acquisitionLoop();
    void processingLoop();
    void serverLoop();
    std::string handleCommand(const std::string& line);
    std::string formatStatus();

    Config cfg_;
    std::unique_ptr<IDevice> dev_;
    BoundedQueue<envmon_sample> queue_;

    std::mutex state_mu_;                 // guards the fields below
    Statistics temp_stats_, hum_stats_;
    AlertEngine alert_;
    Reading last_;
    bool has_last_{false};
    uint64_t processed_{0}, transitions_{0};
    std::ofstream csv_;

    std::atomic<bool> stop_{true};
    int listen_fd_{-1};
    std::thread t_acq_, t_proc_, t_srv_;
    std::chrono::steady_clock::time_point started_;
};

}  // namespace envmon
