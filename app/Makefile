CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic -Wshadow -pthread
CPPFLAGS += -Iinclude -I../include -I../app/include
BUILD    := ../build

LIB_SRC  := $(filter-out src/main.cpp src/ctl.cpp,$(wildcard src/*.cpp))
LIB_OBJ  := $(LIB_SRC:src/%.cpp=$(BUILD)/obj/%.o)

all: $(BUILD)/envmond $(BUILD)/envmonctl $(BUILD)/libenvmon.a

$(BUILD)/obj/%.o: src/%.cpp $(wildcard include/envmon/*.hpp) ../include/envmon_ioctl.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD)/libenvmon.a: $(LIB_OBJ)
	ar rcs $@ $^

$(BUILD)/envmond: $(BUILD)/obj/main.o $(BUILD)/libenvmon.a
	$(CXX) $(CXXFLAGS) $^ -o $@
$(BUILD)/envmonctl: $(BUILD)/obj/ctl.o $(BUILD)/libenvmon.a
	$(CXX) $(CXXFLAGS) $^ -o $@

clean:
	rm -rf $(BUILD)/obj $(BUILD)/envmond $(BUILD)/envmonctl $(BUILD)/libenvmon.a
.PHONY: all clean
