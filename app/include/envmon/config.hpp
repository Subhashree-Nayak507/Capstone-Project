#pragma once
#include <cstdint>
#include <string>
#include "envmon/alert_engine.hpp"
#include "envmon/logger.hpp"

namespace envmon {

struct Config {
    std::string device_path{"/dev/envmon"};
    bool        simulate{false};          // use user-space SimDevice
    uint32_t    period_ms{500};
    std::string socket_path{"/tmp/envmon.sock"};
    std::string log_file;
    std::string csv_file;
    std::size_t queue_capacity{1024};
    std::size_t window{20};
    uint64_t    max_samples{0};           // 0 = run until signalled
    bool        daemonize{false};
    LogLevel    log_level{LogLevel::Info};
    Thresholds  thresholds;

    /// Apply a single option (used by both the config file and the CLI).
    bool set(const std::string& key, const std::string& value, std::string& err);
    /// Load "key = value" lines ('#' comments allowed).
    bool loadFile(const std::string& path, std::string& err);
    /// Validate ranges; fills err on failure.
    bool validate(std::string& err) const;
};

}  
