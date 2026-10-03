#include "envmon/device.hpp"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>

namespace envmon {

// ------------------------------------------------------------ KernelDevice --
bool KernelDevice::open(std::string& err) {
    fd_ = ::open(path_.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd_ < 0) {
        err = "open(" + path_ + "): " + std::strerror(errno);
        return false;
    }
    return true;
}

void KernelDevice::close() {
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
}

int KernelDevice::read(std::vector<envmon_sample>& out, int timeout_ms) {
    if (fd_ < 0) return -1;
    pollfd p{fd_, POLLIN, 0};
    int rc = ::poll(&p, 1, timeout_ms);
    if (rc < 0) return errno == EINTR ? 0 : -1;
    if (rc == 0) return 0;
    if (p.revents & (POLLERR | POLLNVAL | POLLHUP)) return -1;

    envmon_sample buf[32];
    ssize_t n = ::read(fd_, buf, sizeof(buf));
    if (n < 0) return (errno == EAGAIN || errno == EINTR) ? 0 : -1;
    int count = static_cast<int>(n / static_cast<ssize_t>(sizeof(envmon_sample)));
    out.insert(out.end(), buf, buf + count);
    return count;
}

bool KernelDevice::setPeriod(uint32_t ms) {
    return fd_ >= 0 && ::ioctl(fd_, ENVMON_IOC_SET_PERIOD, &ms) == 0;
}
bool KernelDevice::getStats(envmon_stats& st) {
    return fd_ >= 0 && ::ioctl(fd_, ENVMON_IOC_GET_STATS, &st) == 0;
}
bool KernelDevice::reset() {
    return fd_ >= 0 && ::ioctl(fd_, ENVMON_IOC_RESET) == 0;
}

// --------------------------------------------------------------- SimDevice --
SimDevice::SimDevice(uint32_t period_ms, uint32_t seed)
    : rng_(seed), period_ms_(period_ms) {}

bool SimDevice::open(std::string&) {
    std::lock_guard<std::mutex> lk(mu_);
    next_ = Clock::now() + std::chrono::milliseconds(period_ms_);
    return true;
}

envmon_sample SimDevice::generate() {   // caller holds mu_
    std::uniform_int_distribution<int> dt(-150, 150), dh(-300, 300);
    temp_mC_ = std::clamp(temp_mC_ + dt(rng_), 15000, 45000);
    hum_mpct_ = std::clamp(hum_mpct_ + dh(rng_), 20000, 90000);
    envmon_sample s{};
    s.timestamp_ns = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now().time_since_epoch()).count());
    s.temp_mC = temp_mC_;
    s.humidity_mpct = hum_mpct_;
    s.seq = seq_++;
    ++generated_;
    return s;
}

int SimDevice::read(std::vector<envmon_sample>& out, int timeout_ms) {
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
    for (;;) {
        Clock::time_point wake;
        {
            std::lock_guard<std::mutex> lk(mu_);
            auto now = Clock::now();
            if (now >= next_) {
                int n = 0;
                const auto step = std::chrono::milliseconds(period_ms_);
                while (now >= next_ && n < 32) { out.push_back(generate()); next_ += step; ++n; }
                if (now >= next_) next_ = now + step;   // skip missed periods
                return n;
            }
            wake = next_;
        }
        auto now = Clock::now();
        if (now >= deadline) return 0;
        std::this_thread::sleep_until(std::min({wake, deadline, now + std::chrono::milliseconds(50)}));
    }
}

bool SimDevice::setPeriod(uint32_t ms) {
    if (ms < ENVMON_MIN_PERIOD_MS || ms > ENVMON_MAX_PERIOD_MS) return false;
    std::lock_guard<std::mutex> lk(mu_);
    period_ms_ = ms;
    next_ = Clock::now() + std::chrono::milliseconds(ms);
    return true;
}

bool SimDevice::getStats(envmon_stats& st) {
    std::lock_guard<std::mutex> lk(mu_);
    st = {};
    st.generated = generated_;
    st.period_ms = period_ms_;
    return true;
}

bool SimDevice::reset() {
    std::lock_guard<std::mutex> lk(mu_);
    seq_ = 0; generated_ = 0;
    return true;
}

std::unique_ptr<IDevice> makeDevice(const Config& cfg) {
    if (cfg.simulate) return std::make_unique<SimDevice>(cfg.period_ms);
    return std::make_unique<KernelDevice>(cfg.device_path);
}

}  // namespace envmon
