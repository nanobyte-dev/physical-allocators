LOCAL_BUILD_DIR=$(BUILD_DIR)/benchmark

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

.PHONY: all benchmark clean

all: benchmark

benchmark: $(LOCAL_BUILD_DIR)/benchmark

$(LOCAL_BUILD_DIR)/benchmark: $(OBJECTS_CPP) $(STATIC_LIBS)
	@echo "Linking $(@F)..."
	@$(CXX) $(CXXFLAGS) -o $@ $^

$(LOCAL_BUILD_DIR)/%.o: %.cpp $(HEADERS)
	@echo "Building $(@F)..."
	@mkdir -p $(@D)
	@$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	@rm -rf $(LOCAL_BUILD_DIR)/*