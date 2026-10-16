# Host builds: unit tests and the terminal simulator. Firmware builds with
# PlatformIO from firmware/ (see README.md).

CXX ?= c++
CXXFLAGS ?= -std=c++17 -O1 -g -Wall -Wextra -Werror
CORE_DIR := lib/petcore/src
CORE_SRC := $(wildcard $(CORE_DIR)/*.cpp)
CORE_HDR := $(wildcard $(CORE_DIR)/*.h)
BUILD := build

.PHONY: all test sim clean

all: test sim

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/test_petcore: test/test_petcore.cpp $(CORE_SRC) $(CORE_HDR) | $(BUILD)
	$(CXX) $(CXXFLAGS) -I$(CORE_DIR) test/test_petcore.cpp $(CORE_SRC) -o $@

$(BUILD)/petsim: sim/main.cpp $(CORE_SRC) $(CORE_HDR) | $(BUILD)
	$(CXX) $(CXXFLAGS) -I$(CORE_DIR) sim/main.cpp $(CORE_SRC) -o $@

test: $(BUILD)/test_petcore
	./$(BUILD)/test_petcore

sim: $(BUILD)/petsim

clean:
	rm -rf $(BUILD)
