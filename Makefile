export BUILD_DIR=$(abspath build)
export CXX=g++
#export CXXFLAGS=-ggdb -std=c++17 -I. -I$(abspath src) -Wall
export CXXFLAGS=-ggdb -std=c++17 -O2 -march=native -I. -I$(abspath src) -Wall -DMEASURE_WASTE
export AR=ar

.PHONY: all phallocators demo tests benchmark clean .FORCE

all: phallocators demo tests benchmark

phallocators: $(BUILD_DIR)/phallocators/libphallocators.a

$(BUILD_DIR)/phallocators/libphallocators.a: .FORCE
	@$(MAKE) -C src/phallocators


demo: $(BUILD_DIR)/demo/demo

$(BUILD_DIR)/demo/demo: phallocators .FORCE
	@$(MAKE) -C src/demo


tests: $(BUILD_DIR)/tests/tests

$(BUILD_DIR)/tests/tests: phallocators .FORCE
	@$(MAKE) -C src/tests

benchmark: $(BUILD_DIR)/benchmark/benchmark

$(BUILD_DIR)/benchmark/benchmark: phallocators .FORCE
	@$(MAKE) -C src/benchmark


clean:
	@$(MAKE) -C src/phallocators clean
	@$(MAKE) -C src/demo clean
	@$(MAKE) -C src/tests clean
	@$(MAKE) -C src/benchmark clean
	@rm -rf $(BUILD_DIR)/*
