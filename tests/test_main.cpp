#include "test_framework.hpp"
int main() {
    int failedTests = 0;
    for (auto& t : registry()) {
        int before = failures();
        t.fn();
        bool ok = failures() == before;
        std::cout << (ok ? "[ PASS ] " : "[ FAIL ] ") << t.name << '\n';
        if (!ok) ++failedTests;
    }
    std::cout << "\n" << registry().size() - failedTests << "/" << registry().size() << " tests passed\n";
    return failedTests ? 1 : 0;
}
