#include <fstream>
#include <thread>
#include <unistd.h>
#include "test_framework.hpp"
#include "envmon/ipc.hpp"
#include "envmon/monitor.hpp"

using namespace envmon;
using namespace std::chrono_literals;

static Config simConfig(const std::string& tag) {
    Config c; c.simulate = true; c.period_ms = 10;
    c.socket_path = "/tmp/envmon_it_" + tag + "_" + std::to_string(getpid()) + ".sock";
    Logger::instance().setLevel(LogLevel::Error);
    return c;
}
static bool waitFor(Monitor& m, uint64_t n) {
    for (int i = 0; i < 300; ++i) { if (m.snapshot().processed >= n) return true; std::this_thread::sleep_for(10ms); }
    return false;
}

TEST(it_pipeline_processes_samples) {
    Config c = simConfig("pipe");
    Monitor m(c, makeDevice(c));
    std::string err; CHECK(m.start(err));
    CHECK(waitFor(m, 20));
    Snapshot s = m.snapshot();
    CHECK(s.has_last);
    CHECK(s.temp_min <= s.temp_mean && s.temp_mean <= s.temp_max);
    m.requestStop(); m.join();
}

TEST(it_socket_commands) {
    Config c = simConfig("cmd");
    Monitor m(c, makeDevice(c));
    std::string err, reply; CHECK(m.start(err));
    CHECK(waitFor(m, 5));
    CHECK(sendCommand(c.socket_path, "PING", reply, err)); CHECK_EQ(reply, std::string("PONG\n"));
    CHECK(sendCommand(c.socket_path, "STATUS", reply, err));
    CHECK(reply.find("samples=") != std::string::npos);
    CHECK(reply.find("state=NORMAL") != std::string::npos || reply.find("state=") != std::string::npos);
    CHECK(sendCommand(c.socket_path, "LAST", reply, err)); CHECK(reply.rfind("seq=", 0) == 0);
    CHECK(sendCommand(c.socket_path, "SET_PERIOD 25", reply, err)); CHECK_EQ(reply, std::string("OK\n"));
    CHECK(sendCommand(c.socket_path, "SET_PERIOD 1", reply, err)); CHECK(reply.rfind("ERR", 0) == 0);
    CHECK(sendCommand(c.socket_path, "NONSENSE", reply, err));     CHECK(reply.rfind("ERR", 0) == 0);
    CHECK(sendCommand(c.socket_path, "RESET", reply, err));        CHECK_EQ(reply, std::string("OK\n"));
    m.requestStop(); m.join();
    CHECK(!sendCommand(c.socket_path, "PING", reply, err));        // socket removed
}

TEST(it_quit_command_stops_monitor) {
    Config c = simConfig("quit");
    Monitor m(c, makeDevice(c));
    std::string err, reply; CHECK(m.start(err));
    CHECK(sendCommand(c.socket_path, "QUIT", reply, err));
    CHECK_EQ(reply, std::string("BYE\n"));
    for (int i = 0; i < 100 && m.running(); ++i) std::this_thread::sleep_for(10ms);
    CHECK(!m.running());
    m.join();
}

TEST(it_max_samples_auto_stop) {
    Config c = simConfig("max"); c.max_samples = 15;
    Monitor m(c, makeDevice(c));
    std::string err; CHECK(m.start(err));
    for (int i = 0; i < 300 && m.running(); ++i) std::this_thread::sleep_for(10ms);
    CHECK(!m.running());
    m.join();
    CHECK(m.snapshot().processed >= 15);
}

TEST(it_alert_transition_with_low_thresholds) {
    Config c = simConfig("alert");
    c.thresholds.warn_temp = 10; c.thresholds.crit_temp = 11;   // always Critical
    Monitor m(c, makeDevice(c));
    std::string err; CHECK(m.start(err));
    CHECK(waitFor(m, 3));
    Snapshot s = m.snapshot();
    CHECK(s.state == AlertState::Critical);
    CHECK(s.alert_transitions >= 1);
    m.requestStop(); m.join();
}

TEST(it_csv_output) {
    Config c = simConfig("csv"); c.csv_file = "/tmp/envmon_it_" + std::to_string(getpid()) + ".csv";
    ::unlink(c.csv_file.c_str());
    Monitor m(c, makeDevice(c));
    std::string err; CHECK(m.start(err));
    CHECK(waitFor(m, 5));
    m.requestStop(); m.join();
    std::ifstream f(c.csv_file);
    std::string header; std::getline(f, header);
    CHECK_EQ(header, std::string("seq,timestamp_ns,temp_c,humidity_pct,state"));
    std::string row; CHECK(static_cast<bool>(std::getline(f, row)));
    ::unlink(c.csv_file.c_str());
}

TEST(it_missing_device_fails_to_start) {
    Config c; c.device_path = "/dev/nonexistent_envmon"; c.socket_path = "/tmp/envmon_it_nodev.sock";
    Monitor m(c, makeDevice(c));
    std::string err;
    CHECK(!m.start(err));
    CHECK(!err.empty());
}
