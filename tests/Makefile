CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O1 -g -Wall -Wextra -pthread
CPPFLAGS += -I../app/include -I../include
BUILD    := ../build

$(BUILD)/envmon_tests: test_main.cpp test_unit.cpp test_integration.cpp $(BUILD)/libenvmon.a
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $^ -o $@

$(BUILD)/libenvmon.a:
	$(MAKE) -C ../app

run: $(BUILD)/envmon_tests
	$(BUILD)/envmon_tests
.PHONY: run
