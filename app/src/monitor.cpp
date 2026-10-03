#include "envmon/monitor.hpp"
#include <cerrno>
#include <cstring>
#include <iomanip>
#include <poll.h>
#include <sstream>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace envmon {
using namespace std::chrono_literals;

Monitor::Monitor(Config cfg, std::unique_ptr<IDevice> dev)
    : cfg_(std::move(cfg)), dev_(std::move(dev)), queue_(cfg_.queue_capacity),
      temp_stats_(cfg_.window), hum_stats_(cfg_.window), alert_(cfg_.thresholds) {}

Monitor::~Monitor() { requestStop(); join(); }

bool Monitor::start(std::string& err) {
    if (!dev_->open(err)) return false;
    if (!cfg_.simulate && !dev_->setPeriod(cfg_.period_ms))
        LOG_WARN("could not set device period to " << cfg_.period_ms << " ms; using driver default");

    if (!cfg_.csv_file.empty()) {
        csv_.open(cfg_.csv_file, std::ios::app);
        if (!csv_) { err = "cannot open CSV file '" + cfg_.csv_file + "'"; return false; }
        csv_ << "seq,timestamp_ns,temp_c,humidity_pct,state\n";
    }

    // Unix-domain control socket
    listen_fd_ = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (listen_fd_ < 0) { err = std::string("socket: ") + std::strerror(errno); return false; }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, cfg_.socket_path.c_str(), sizeof(addr.sun_path) - 1);
    ::unlink(cfg_.socket_path.c_str());
    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 ||
        ::listen(listen_fd_, 8) < 0) {
        err = "bind/listen(" + cfg_.socket_path + "): " + std::strerror(errno);
        ::close(listen_fd_); listen_fd_ = -1;
        return false;
    }
    ::chmod(cfg_.socket_path.c_str(), 0660);

    started_ = std::chrono::steady_clock::now();
    stop_.store(false);
    t_acq_  = std::thread(&Monitor::acquisitionLoop, this);
    t_proc_ = std::thread(&Monitor::processingLoop, this);
    t_srv_  = std::thread(&Monitor::serverLoop, this);
    LOG_INFO("monitor started (device=" << dev_->name() << ", period=" << cfg_.period_ms
             << " ms, socket=" << cfg_.socket_path << ")");
    return true;
}

void Monitor::requestStop() {
    stop_.store(true);
    queue_.close();
}

void Monitor::join() {
    if (t_acq_.joinable())  t_acq_.join();
    if (t_proc_.joinable()) t_proc_.join();
    if (t_srv_.joinable())  t_srv_.join();
    if (listen_fd_ >= 0) {
        ::close(listen_fd_); listen_fd_ = -1;
        ::unlink(cfg_.socket_path.c_str());
        dev_->close();
        if (csv_.is_open()) csv_.close();
        LOG_INFO("monitor stopped");
    }
}

void Monitor::acquisitionLoop() {
    std::vector<envmon_sample> batch;
    int consecutiveErrors = 0;
    while (!stop_.load()) {
        batch.clear();
        int n = dev_->read(batch, 200);
        if (n < 0) {
            LOG_ERROR("device read failed");
            if (++consecutiveErrors >= 5) { LOG_ERROR("too many device errors, stopping"); requestStop(); break; }
            std::this_thread::sleep_for(200ms);
            continue;
        }
        consecutiveErrors = 0;
        for (const auto& s : batch) {
            if (s.flags & ENVMON_F_OVERRUN) LOG_WARN("kernel ring overrun before seq " << s.seq);
            if (!queue_.push(s)) LOG_DEBUG("user queue full, dropped oldest sample");
        }
    }
}

void Monitor::processingLoop() {
    envmon_sample raw;
    while (true) {
        if (!queue_.pop(raw, 100ms)) {
            if (stop_.load()) break;
            continue;
        }
        Reading r = toReading(raw);
        std::optional<Transition> tr;
        AlertState st;
        {
            std::lock_guard<std::mutex> lk(state_mu_);
            temp_stats_.add(r.temp_c);
            hum_stats_.add(r.humidity_pct);
            last_ = r; has_last_ = true;
            tr = alert_.update(r);
            st = alert_.state();
            if (tr) ++transitions_;
            ++processed_;
            if (csv_.is_open())
                csv_ << r.seq << ',' << r.timestamp_ns << ',' << std::fixed << std::setprecision(3)
                     << r.temp_c << ',' << r.humidity_pct << ',' << toString(st) << '\n';
        }
        if (tr) {
            auto lvl = tr->to > tr->from ? LogLevel::Warn : LogLevel::Info;
            ENVMON_LOG(lvl, "ALERT " << toString(tr->from) << " -> " << toString(tr->to)
                       << " (temp=" << tr->reading.temp_c << " C, hum=" << tr->reading.humidity_pct << " %)");
        }
        if (cfg_.max_samples && snapshot().processed >= cfg_.max_samples) requestStop();
    }
}

void Monitor::serverLoop() {
    while (!stop_.load()) {
        pollfd p{listen_fd_, POLLIN, 0};
        if (::poll(&p, 1, 100) <= 0) continue;
        int c = ::accept4(listen_fd_, nullptr, nullptr, SOCK_CLOEXEC);
        if (c < 0) continue;

        std::string line;
        char buf[256];
        while (line.find('\n') == std::string::npos && line.size() < 256) {
            pollfd cp{c, POLLIN, 0};
            if (::poll(&cp, 1, 500) <= 0) break;
            ssize_t n = ::recv(c, buf, sizeof(buf), 0);
            if (n <= 0) break;
            line.append(buf, static_cast<std::size_t>(n));
        }
        std::string resp = handleCommand(line);
        const char* data = resp.data();
        std::size_t left = resp.size();
        while (left) {
            ssize_t w = ::send(c, data, left, MSG_NOSIGNAL);
            if (w <= 0) break;
            data += w; left -= static_cast<std::size_t>(w);
        }
        ::close(c);
    }
}

std::string Monitor::handleCommand(const std::string& line) {
    std::istringstream is(line);
    std::string cmd;
    is >> cmd;
    if (cmd == "PING") return "PONG\n";
    if (cmd == "STATUS") return formatStatus();
    if (cmd == "LAST") {
        Snapshot s = snapshot();
        if (!s.has_last) return "ERR no data yet\n";
        std::ostringstream o;
        o << std::fixed << std::setprecision(2) << "seq=" << s.last.seq << " temp_c=" << s.last.temp_c
          << " humidity_pct=" << s.last.humidity_pct << " state=" << toString(s.state) << '\n';
        return o.str();
    }
    if (cmd == "SET_PERIOD") {
        long long ms = -1;
        if (!(is >> ms) || ms < ENVMON_MIN_PERIOD_MS || ms > ENVMON_MAX_PERIOD_MS)
            return "ERR period must be 10..60000 ms\n";
        if (!dev_->setPeriod(static_cast<uint32_t>(ms))) return "ERR device rejected period\n";
        LOG_INFO("sampling period changed to " << ms << " ms via control socket");
        return "OK\n";
    }
    if (cmd == "RESET") {
        dev_->reset();
        std::lock_guard<std::mutex> lk(state_mu_);
        temp_stats_.reset(); hum_stats_.reset();
        processed_ = 0; transitions_ = 0; has_last_ = false;
        return "OK\n";
    }
    if (cmd == "QUIT") { requestStop(); return "BYE\n"; }
    return "ERR unknown command (PING STATUS LAST SET_PERIOD RESET QUIT)\n";
}

Snapshot Monitor::snapshot() {
    Snapshot s;
    {
        std::lock_guard<std::mutex> lk(state_mu_);
        s.processed = processed_;
        s.has_last = has_last_;
        s.last = last_;
        s.state = alert_.state();
        s.alert_transitions = transitions_;
        s.temp_min = temp_stats_.min(); s.temp_max = temp_stats_.max();
        s.temp_mean = temp_stats_.mean(); s.temp_avg_window = temp_stats_.movingAverage();
        s.hum_min = hum_stats_.min(); s.hum_max = hum_stats_.max(); s.hum_mean = hum_stats_.mean();
    }
    s.queue_dropped = queue_.dropped();
    dev_->getStats(s.dev);
    s.uptime_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - started_).count();
    return s;
}

std::string Monitor::formatStatus() {
    Snapshot s = snapshot();
    std::ostringstream o;
    o << std::fixed << std::setprecision(2)
      << "state=" << toString(s.state) << '\n'
      << "uptime_s=" << s.uptime_s << '\n'
      << "samples=" << s.processed << '\n'
      << "device_generated=" << s.dev.generated << '\n'
      << "device_dropped=" << s.dev.dropped << '\n'
      << "queue_dropped=" << s.queue_dropped << '\n'
      << "period_ms=" << s.dev.period_ms << '\n'
      << "alert_transitions=" << s.alert_transitions << '\n'
      << "temp_c_min=" << s.temp_min << '\n' << "temp_c_max=" << s.temp_max << '\n'
      << "temp_c_mean=" << s.temp_mean << '\n' << "temp_c_window_avg=" << s.temp_avg_window << '\n'
      << "humidity_pct_min=" << s.hum_min << '\n' << "humidity_pct_max=" << s.hum_max << '\n'
      << "humidity_pct_mean=" << s.hum_mean << '\n';
    return o.str();
}

}  // namespace envmon
