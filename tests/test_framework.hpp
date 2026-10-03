#pragma once
// Minimal self-contained unit-test framework (no external dependencies).
#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

struct TestCase { std::string name; std::function<void()> fn; };
inline std::vector<TestCase>& registry() { static std::vector<TestCase> r; return r; }
inline int& failures() { static int f = 0; return f; }
struct Registrar { Registrar(const char* n, std::function<void()> f) { registry().push_back({n, std::move(f)}); } };

#define TEST(name) static void name(); static Registrar reg_##name(#name, name); static void name()
#define CHECK(cond) do { if (!(cond)) { ++failures(); \
    std::cerr << "  FAIL " << __FILE__ << ':' << __LINE__ << ": " #cond "\n"; } } while (0)
#define CHECK_EQ(a, b) do { auto _a = (a); auto _b = (b); if (!(_a == _b)) { ++failures(); \
    std::cerr << "  FAIL " << __FILE__ << ':' << __LINE__ << ": " #a " == " #b " (" << _a << " vs " << _b << ")\n"; } } while (0)
#define CHECK_NEAR(a, b, eps) do { double _a = (a), _b = (b); if (std::fabs(_a - _b) > (eps)) { ++failures(); \
    std::cerr << "  FAIL " << __FILE__ << ':' << __LINE__ << ": " #a " ~= " #b " (" << _a << " vs " << _b << ")\n"; } } while (0)
