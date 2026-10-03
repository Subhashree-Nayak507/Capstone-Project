#include <thread>
#include <fstream>
#include "test_framework.hpp"
#include "envmon/alert_engine.hpp"
#include "envmon/bounded_queue.hpp"
#include "envmon/config.hpp"
#include "envmon/device.hpp"
#include "envmon/statistics.hpp"

using namespace envmon;
using namespace std::chrono_literals;

// ---------------------------------------------------------- Statistics ----
TEST(stats_basic) {
    Statistics s(3);
    for (double v : {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0}) s.add(v);
    CHECK_EQ(s.count(), 8u);
    CHECK_NEAR(s.mean(), 5.0, 1e-9);
    CHECK_NEAR(s.stddev(), 2.0, 1e-9);
    CHECK_NEAR(s.min(), 2.0, 1e-9);
    CHECK_NEAR(s.max(), 9.0, 1e-9);
    CHECK_NEAR(s.movingAverage(), (5.0 + 7.0 + 9.0) / 3.0, 1e-9);  // last 3
}
TEST(stats_empty_and_reset) {
    Statistics s;
    CHECK_NEAR(s.mean(), 0.0, 1e-12);
    CHECK_NEAR(s.stddev(), 0.0, 1e-12);
    s.add(10); s.reset();
    CHECK_EQ(s.count(), 0u);
    CHECK_NEAR(s.movingAverage(), 0.0, 1e-12);
}

// -------------------------------------------------------- AlertEngine ----
static Reading R(double t, double h = 50.0) { Reading r; r.temp_c = t; r.humidity_pct = h; return r; }

TEST(alert_escalation) {
    AlertEngine e;
    CHECK(!e.update(R(25)).has_value());
    auto t1 = e.update(R(31));
    CHECK(t1.has_value() && t1->to == AlertState::Warning);
    auto t2 = e.update(R(36));
    CHECK(t2.has_value() && t2->from == AlertState::Warning && t2->to == AlertState::Critical);
}
TEST(alert_hysteresis_prevents_flapping) {
    AlertEngine e;                       // warn 30, crit 35, hyst 1
    e.update(R(31));
    CHECK(!e.update(R(29.5)).has_value());          // inside hysteresis band: stay Warning
    CHECK(e.state() == AlertState::Warning);
    auto t = e.update(R(28.9));
    CHECK(t.has_value() && t->to == AlertState::Normal);
}
TEST(alert_critical_deescalation) {
    AlertEngine e;
    e.update(R(40));
    CHECK(!e.update(R(34.5)).has_value());          // still >= crit - hyst
    auto t = e.update(R(33.9));
    CHECK(t.has_value() && t->to == AlertState::Warning);
}
TEST(alert_humidity_dominates) {
    AlertEngine e;
    auto t = e.update(R(22, 90));
    CHECK(t.has_value() && t->to == AlertState::Critical);
}

// ------------------------------------------------------- BoundedQueue ----
TEST(queue_fifo_and_drop_oldest) {
    BoundedQueue<int> q(3);
    for (int i = 1; i <= 5; ++i) q.push(i);
    CHECK_EQ(q.dropped(), 2u);
    int v = 0;
    CHECK(q.pop(v, 10ms)); CHECK_EQ(v, 3);
    CHECK(q.pop(v, 10ms)); CHECK_EQ(v, 4);
}
TEST(queue_timeout_and_close) {
    BoundedQueue<int> q(2);
    int v;
    CHECK(!q.pop(v, 20ms));
    q.push(7); q.close();
    CHECK(q.pop(v, 10ms)); CHECK_EQ(v, 7);       // drains after close
    CHECK(!q.pop(v, 10ms));
    CHECK(!q.push(9));                            // rejected after close
}
TEST(queue_producer_consumer_threads) {
    BoundedQueue<int> q(10000);
    std::thread prod([&] { for (int i = 0; i < 5000; ++i) q.push(i); q.close(); });
    long long sum = 0; int v;
    while (q.pop(v, 200ms)) sum += v;
    prod.join();
    CHECK_EQ(sum, 5000LL * 4999 / 2);
}

// -------------------------------------------------------------- Config ----
TEST(config_set_and_validate) {
    Config c; std::string err;
    CHECK(c.set("--period_ms", "100", err));
    CHECK(c.set("--warn-temp", "28.5", err));      // dashes normalised
    CHECK(c.set("simulate", "true", err));
    CHECK_EQ(c.period_ms, 100u);
    CHECK_NEAR(c.thresholds.warn_temp, 28.5, 1e-9);
    CHECK(c.simulate);
    CHECK(c.validate(err));
}
TEST(config_rejects_bad_input) {
    Config c; std::string err;
    CHECK(!c.set("period_ms", "abc", err));
    CHECK(!c.set("period_ms", "-5", err));
    CHECK(!c.set("bogus", "1", err));
    c.period_ms = 5;
    CHECK(!c.validate(err));
    c.period_ms = 100; c.thresholds.warn_temp = 40;
    CHECK(!c.validate(err));
}
TEST(config_file_parsing) {
    const char* path = "/tmp/envmon_test.conf";
    { std::ofstream f(path); f << "# comment\nperiod_ms = 250   # inline\n\nwarn_hum=60\n"; }
    Config c; std::string err;
    CHECK(c.loadFile(path, err));
    CHECK_EQ(c.period_ms, 250u);
    CHECK_NEAR(c.thresholds.warn_hum, 60.0, 1e-9);
    { std::ofstream f(path); f << "this is not valid\n"; }
    CHECK(!c.loadFile(path, err));
    CHECK(!c.loadFile("/nonexistent/file.conf", err));
}

// ----------------------------------------------------------- SimDevice ----
TEST(sim_device_generates_in_range_and_sequence) {
    SimDevice d(10);
    std::string err; CHECK(d.open(err));
    std::vector<envmon_sample> v;
    auto t0 = std::chrono::steady_clock::now();
    while (v.size() < 10 && std::chrono::steady_clock::now() - t0 < 2s) d.read(v, 50);
    CHECK(v.size() >= 10);
    for (size_t i = 0; i < v.size(); ++i) {
        CHECK_EQ(v[i].seq, static_cast<uint32_t>(i));
        CHECK(v[i].temp_mC >= 15000 && v[i].temp_mC <= 45000);
        CHECK(v[i].humidity_mpct >= 20000 && v[i].humidity_mpct <= 90000);
    }
}
TEST(sim_device_period_and_reset) {
    SimDevice d(100);
    std::string err; d.open(err);
    CHECK(!d.setPeriod(5));
    CHECK(d.setPeriod(50));
    envmon_stats st{}; d.getStats(st);
    CHECK_EQ(st.period_ms, 50u);
    CHECK(d.reset());
}
TEST(sim_device_read_timeout_returns_zero) {
    SimDevice d(1000);
    std::string err; d.open(err);
    std::vector<envmon_sample> v;
    CHECK_EQ(d.read(v, 30), 0);
}
TEST(kernel_device_missing_node_fails_cleanly) {
    KernelDevice d("/dev/does_not_exist_envmon");
    std::string err;
    CHECK(!d.open(err));
    CHECK(!err.empty());
    std::vector<envmon_sample> v;
    CHECK_EQ(d.read(v, 10), -1);
}
