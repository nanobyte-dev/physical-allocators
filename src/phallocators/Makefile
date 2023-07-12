LOCAL_BUILD_DIR=$(BUILD_DIR)/phallocators

SOURCES_CPP=$(wildcard *.cpp) \
		    $(wildcard */*.cpp) \
			$(wildcard */*/*.cpp) \
			$(wildcard */*/*/*.cpp)

OBJECTS_CPP=$(patsubst %.cpp, $(LOCAL_BUILD_DIR)/%.o, $(SOURCES_CPP))

HEADERS=$(wildcard *.h) \
		$(wildcard */*.h) \
		$(wildcard */*/*.h) \
		$(wildcard */*/*/*.h) \
		$(wildcard *.hpp) \
		$(wildcard */*.hpp) \
		$(wildcard */*/*.hpp) \
		$(wildcard */*/*/*.hpp) \

.PHONY: all libphallocators clean

all: libphallocators

libphallocators: $(LOCAL_BUILD_DIR)/libphallocators.a

$(LOCAL_BUILD_DIR)/libphallocators.a: $(OBJECTS_CPP)
	@echo "Building static library $(@F)..."
	@$(AR) rcs $@ $^

$(LOCAL_BUILD_DIR)/%.o: %.cpp $(HEADERS)
	@echo "Building $(@F)..."
	@mkdir -p $(@D)
	@$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	@rm -rf $(LOCAL_BUILD_DIR)/*