export BUILD_DIR=$(abspath build)
export CXX=g++
#export CXXFLAGS=-ggdb -I. -I$(abspath src) -Wall
export CXXFLAGS=-O2 -I. -I$(abspath src) -Wall
export AR=ar

.PHONY: all fat clean always .FORCE

all: phallocators demo tests

phallocators: $(BUILD_DIR)/phallocators/libphallocators.a

$(BUILD_DIR)/phallocators/libphallocators.a: .FORCE
	@$(MAKE) -C src/phallocators


demo: $(BUILD_DIR)/demo/demo

$(BUILD_DIR)/demo/demo: phallocators .FORCE
	@$(MAKE) -C src/demo


tests: $(BUILD_DIR)/tests/tests

$(BUILD_DIR)/tests/tests: phallocators .FORCE
	@$(MAKE) -C src/tests


clean:
	@$(MAKE) -C src/phallocators clean
	@$(MAKE) -C src/demo clean
	@$(MAKE) -C src/tests clean
	@rm -rf $(BUILD_DIR)/*
