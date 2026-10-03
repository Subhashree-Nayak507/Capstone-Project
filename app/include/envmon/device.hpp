#pragma once
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <vector>
#include "envmon/config.hpp"

namespace envmon {

/// Abstract sensor source: the real kernel driver or a software simulation.
class IDevice {
public:
    virtual ~IDevice() = default;
    virtual bool open(std::string& err) = 0;
    virtual void close() = 0;
    /// Append up to a batch of samples to @p out, waiting at most @p timeout_ms.
    /// @return number of samples (0 = timeout), or -1 on a fatal error.
    virtual int  read(std::vector<envmon_sample>& out, int timeout_ms) = 0;
    virtual bool setPeriod(uint32_t ms) = 0;
    virtual bool getStats(envmon_stats& st) = 0;
    virtual bool reset() = 0;
    virtual const char* name() const = 0;
};

/// Talks to /dev/envmon via poll()/read()/ioctl(). RAII owner of the fd.
class KernelDevice : public IDevice {
public:
    explicit KernelDevice(std::string path) : path_(std::move(path)) {}
    ~KernelDevice() override { close(); }
    KernelDevice(const KernelDevice&) = delete;
    KernelDevice& operator=(const KernelDevice&) = delete;

    bool open(std::string& err) override;
    void close() override;
    int  read(std::vector<envmon_sample>& out, int timeout_ms) override;
    bool setPeriod(uint32_t ms) override;
    bool getStats(envmon_stats& st) override;
    bool reset() override;
    const char* name() const override { return "kernel"; }

private:
    std::string path_;
    int fd_{-1};
};

/// Software stand-in with identical semantics; lets the whole application run
/// and be tested on machines without the kernel module.
class SimDevice : public IDevice {
public:
    explicit SimDevice(uint32_t period_ms = 500, uint32_t seed = 12345);
    bool open(std::string&) override;
    void close() override {}
    int  read(std::vector<envmon_sample>& out, int timeout_ms) override;
    bool setPeriod(uint32_t ms) override;
    bool getStats(envmon_stats& st) override;
    bool reset() override;
    const char* name() const override { return "simulated"; }

private:
    using Clock = std::chrono::steady_clock;
    envmon_sample generate();

    std::mutex mu_;
    std::mt19937 rng_;
    uint32_t period_ms_;
    Clock::time_point next_;
    uint32_t seq_{0};
    uint64_t generated_{0};
    int32_t temp_mC_{25000}, hum_mpct_{50000};
};

std::unique_ptr<IDevice> makeDevice(const Config& cfg);

}  // namespace envmon
