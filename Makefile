BUILD_DIR?=build
CXX?=g++
CXXFLAGS?=-ggdb -Isrc -Wall
#CXXFLAGS?=-O2 -Isrc -Wall

SOURCES_C=$(wildcard *.cpp) $(wildcard */*.cpp) $(wildcard */*/*.cpp)
OBJECTS_C=$(patsubst %.cpp, $(BUILD_DIR)/%.o, $(SOURCES_C))

.PHONY: all fat clean always

all: allocators_test

allocators_test: $(BUILD_DIR)/allocators_test

$(BUILD_DIR)/allocators_test: $(OBJECTS_C)
	@$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS_C)

$(BUILD_DIR)/%.o: %.cpp always
	@mkdir -p $(@D)
	@$(CXX) $(CXXFLAGS) -c -o $@ $<

always:
	@mkdir -p $(BUILD_DIR)

clean:
	@rm -rf $(BUILD_DIR)/*