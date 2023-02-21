LOCAL_BUILD_DIR=$(BUILD_DIR)/tests

SOURCES_CPP=$(wildcard *.cpp) \
		    $(wildcard */*.cpp) \
			$(wildcard */*/*.cpp) \
			$(wildcard */*/*/*.cpp)

HEADERS=$(wildcard *.h) \
		$(wildcard */*.h) \
		$(wildcard */*/*.h) \
		$(wildcard */*/*/*.h) \
		$(wildcard *.hpp) \
		$(wildcard */*.hpp) \
		$(wildcard */*/*.hpp) \
		$(wildcard */*/*/*.hpp) \

OBJECTS_CPP=$(patsubst %.cpp, $(LOCAL_BUILD_DIR)/%.o, $(SOURCES_CPP))

STATIC_LIBS=$(BUILD_DIR)/phallocators/libphallocators.a

CXXFLAGS+=-DCATCH_AMALGAMATED_CUSTOM_MAIN

.PHONY: all tests clean

all: tests

tests: $(LOCAL_BUILD_DIR)/tests

$(LOCAL_BUILD_DIR)/tests: $(OBJECTS_CPP) $(STATIC_LIBS)
	@echo "Linking $(@F)..."
	@$(CXX) $(CXXFLAGS) -o $@ $^

$(LOCAL_BUILD_DIR)/%.o: %.cpp $(HEADERS)
	@echo "Building $(@F)..."
	@mkdir -p $(@D)
	@$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	@rm -rf $(LOCAL_BUILD_DIR)/*