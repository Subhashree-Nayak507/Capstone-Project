// envmond - EnvMon monitoring daemon
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include "envmon/monitor.hpp"

using namespace envmon;

namespace {
volatile std::sig_atomic_t g_signal = 0;
void onSignal(int sig) { g_signal = sig; }

void usage(const char* prog) {
    std::cout <<
"Usage: " << prog << " [options]\n"
"  --config FILE        read 'key = value' options from FILE\n"
"  --device PATH        sensor device (default /dev/envmon)\n"
"  --simulate           use the built-in software sensor (no kernel module)\n"
"  --period_ms N        sampling period, 10..60000 (default 500)\n"
"  --socket PATH        control socket (default /tmp/envmon.sock)\n"
"  --log_file FILE      also log to FILE\n"
"  --csv_file FILE      append every sample to FILE (CSV)\n"
"  --max_samples N      stop after N samples (0 = run forever)\n"
"  --warn_temp/--crit_temp/--warn_hum/--crit_hum/--hysteresis X\n"
"  --log_level L        debug|info|warn|error\n"
"  --daemon             run in background\n"
"  --help\n";
}

bool daemonizeProcess() {
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid > 0) _exit(0);
    if (setsid() < 0) return false;
    pid = fork();
    if (pid < 0) return false;
    if (pid > 0) _exit(0);
    umask(022);
    int fd = open("/dev/null", O_RDWR);
    if (fd >= 0) { dup2(fd, 0); dup2(fd, 1); dup2(fd, 2); if (fd > 2) close(fd); }
    return true;
}
}  // namespace

int main(int argc, char** argv) {
    Config cfg;
    std::string err;
    // pass 1: --config so that explicit CLI options override the file
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--config" && i + 1 < argc && !cfg.loadFile(argv[i + 1], err)) {
            std::cerr << "error: " << err << '\n'; return 2;
        }
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") { usage(argv[0]); return 0; }
        if (a == "--config") { ++i; continue; }
        if (a == "--simulate" || a == "--daemon") { cfg.set(a, "true", err); continue; }
        if (a.rfind("--", 0) != 0 || i + 1 >= argc) { std::cerr << "error: bad argument '" << a << "'\n"; usage(argv[0]); return 2; }
        if (!cfg.set(a, argv[++i], err)) { std::cerr << "error: " << err << '\n'; return 2; }
    }
    if (!cfg.validate(err)) { std::cerr << "error: " << err << '\n'; return 2; }

    if (cfg.daemonize && !daemonizeProcess()) { std::cerr << "error: daemonize failed\n"; return 1; }
    Logger::instance().setLevel(cfg.log_level);
    if (cfg.daemonize) Logger::instance().setConsole(false);
    if (!cfg.log_file.empty() && !Logger::instance().setFile(cfg.log_file)) {
        std::cerr << "error: cannot open log file\n"; return 1;
    }

    struct sigaction sa{};
    sa.sa_handler = onSignal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    signal(SIGPIPE, SIG_IGN);

    Monitor mon(cfg, makeDevice(cfg));
    if (!mon.start(err)) { LOG_ERROR("startup failed: " << err); return 1; }

    while (!g_signal && mon.running()) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (g_signal) LOG_INFO("signal " << g_signal << " received, shutting down");
    mon.requestStop();
    mon.join();
    return 0;
}
