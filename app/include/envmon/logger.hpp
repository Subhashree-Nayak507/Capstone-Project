#pragma once
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>

namespace envmon {

enum class LogLevel { Debug = 0, Info, Warn, Error };

/// Thread-safe singleton logger writing to stderr and optionally a file.
class Logger {
public:
    static Logger& instance();
    void setLevel(LogLevel l);
    bool setFile(const std::string& path);   // append mode
    void setConsole(bool on);
    void log(LogLevel l, const std::string& msg);
    static bool parseLevel(const std::string& s, LogLevel& out);

private:
    Logger() = default;
    std::mutex   mu_;
    std::ofstream file_;
    LogLevel     level_{LogLevel::Info};
    bool         console_{true};
};

}  // namespace envmon

#define ENVMON_LOG(lvl, expr)                                   \
    do {                                                        \
        std::ostringstream _envmon_os;                          \
        _envmon_os << expr;                                     \
        ::envmon::Logger::instance().log(lvl, _envmon_os.str());\
    } while (0)
#define LOG_DEBUG(expr) ENVMON_LOG(::envmon::LogLevel::Debug, expr)
#define LOG_INFO(expr)  ENVMON_LOG(::envmon::LogLevel::Info, expr)
#define LOG_WARN(expr)  ENVMON_LOG(::envmon::LogLevel::Warn, expr)
#define LOG_ERROR(expr) ENVMON_LOG(::envmon::LogLevel::Error, expr)
