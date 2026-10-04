#include "envmon/config.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace envmon {
namespace {
std::string trim(const std::string& s) {
    auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}
bool parseBool(const std::string& v, bool& out) {
    if (v == "1" || v == "true" || v == "yes" || v == "on") { out = true; return true; }
    if (v == "0" || v == "false" || v == "no" || v == "off") { out = false; return true; }
    return false;
}
}  

bool Config::set(const std::string& rawKey, const std::string& value, std::string& err) {
    std::string key = rawKey;
    key.erase(0, key.find_first_not_of('-'));
    std::replace(key.begin(), key.end(), '-', '_');
    try {
        std::size_t pos = 0;
        auto num = [&](const std::string& s) {
            double d = std::stod(s, &pos);
            if (pos != s.size()) throw std::invalid_argument("trailing");
            return d;
        };
        auto uns = [&](const std::string& s) { unsigned long long u = std::stoull(s, &pos);
            if (pos != s.size() || s.find('-') != std::string::npos) throw std::invalid_argument("bad");
            return u; };
        if (key == "device") device_path = value;
        else if (key == "simulate") { if (!parseBool(value, simulate)) throw std::invalid_argument("bool"); }
        else if (key == "daemon") { if (!parseBool(value, daemonize)) throw std::invalid_argument("bool"); }
        else if (key == "period_ms") period_ms = static_cast<uint32_t>(uns(value));
        else if (key == "socket") socket_path = value;
        else if (key == "log_file") log_file = value;
        else if (key == "csv_file") csv_file = value;
        else if (key == "queue_capacity") queue_capacity = uns(value);
        else if (key == "window") window = uns(value);
        else if (key == "max_samples") max_samples = uns(value);
        else if (key == "log_level") { if (!Logger::parseLevel(value, log_level)) throw std::invalid_argument("level"); }
        else if (key == "warn_temp") thresholds.warn_temp = num(value);
        else if (key == "crit_temp") thresholds.crit_temp = num(value);
        else if (key == "warn_hum") thresholds.warn_hum = num(value);
        else if (key == "crit_hum") thresholds.crit_hum = num(value);
        else if (key == "hysteresis") thresholds.hysteresis = num(value);
        else { err = "unknown option '" + rawKey + "'"; return false; }
    } catch (const std::exception&) {
        err = "invalid value '" + value + "' for option '" + rawKey + "'";
        return false;
    }
    return true;
}

bool Config::loadFile(const std::string& path, std::string& err) {
    std::ifstream in(path);
    if (!in) { err = "cannot open config file '" + path + "'"; return false; }
    std::string line;
    int lineno = 0;
    while (std::getline(in, line)) {
        ++lineno;
        auto hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        line = trim(line);
        if (line.empty()) continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) {
            err = path + ":" + std::to_string(lineno) + ": expected key = value";
            return false;
        }
        std::string e;
        if (!set(trim(line.substr(0, eq)), trim(line.substr(eq + 1)), e)) {
            err = path + ":" + std::to_string(lineno) + ": " + e;
            return false;
        }
    }
    return true;
}

bool Config::validate(std::string& err) const {
    if (period_ms < ENVMON_MIN_PERIOD_MS || period_ms > ENVMON_MAX_PERIOD_MS) {
        err = "period_ms must be in [10, 60000]"; return false; }
    if (queue_capacity == 0) { err = "queue_capacity must be > 0"; return false; }
    if (window == 0) { err = "window must be > 0"; return false; }
    if (thresholds.warn_temp >= thresholds.crit_temp) { err = "warn_temp must be < crit_temp"; return false; }
    if (thresholds.warn_hum >= thresholds.crit_hum) { err = "warn_hum must be < crit_hum"; return false; }
    if (thresholds.hysteresis < 0) { err = "hysteresis must be >= 0"; return false; }
    if (socket_path.size() >= 100) { err = "socket path too long"; return false; }
    return true;
}

}  
