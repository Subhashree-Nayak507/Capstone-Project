#include "envmon/logger.hpp"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>

namespace envmon {

Logger& Logger::instance() { static Logger l; return l; }

void Logger::setLevel(LogLevel l) { std::lock_guard<std::mutex> lk(mu_); level_ = l; }
void Logger::setConsole(bool on) { std::lock_guard<std::mutex> lk(mu_); console_ = on; }

bool Logger::setFile(const std::string& path) {
    std::lock_guard<std::mutex> lk(mu_);
    if (file_.is_open()) file_.close();
    if (path.empty()) return true;
    file_.open(path, std::ios::app);
    return file_.is_open();
}

bool Logger::parseLevel(const std::string& s, LogLevel& out) {
    if (s == "debug") out = LogLevel::Debug;
    else if (s == "info") out = LogLevel::Info;
    else if (s == "warn") out = LogLevel::Warn;
    else if (s == "error") out = LogLevel::Error;
    else return false;
    return true;
}

void Logger::log(LogLevel l, const std::string& msg) {
    static const char* names[] = {"DEBUG", "INFO ", "WARN ", "ERROR"};
    std::lock_guard<std::mutex> lk(mu_);
    if (l < level_) return;
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&t, &tm);
    std::ostringstream line;
    line << std::put_time(&tm, "%F %T") << " [" << names[static_cast<int>(l)] << "] " << msg;
    if (console_) std::cerr << line.str() << '\n';
    if (file_.is_open()) file_ << line.str() << std::endl;
}

}  
