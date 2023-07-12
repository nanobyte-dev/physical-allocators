LOCAL_BUILD_DIR=$(BUILD_DIR)/demo

SOURCES_CPP=$(wildcard *.cpp) \
		    $(wildcard */*.cpp) \
			$(wildcard */*/*.cpp) \
			$(wildcard */*/*/*.cpp)

OBJECTS_CPP=$(patsubst %.cpp, $(LOCAL_BUILD_DIR)/%.o, $(SOURCES_CPP))

STATIC_LIBS=$(BUILD_DIR)/phallocators/libphallocators.a

.PHONY: all demo clean

all: demo

demo: $(LOCAL_BUILD_DIR)/demo

$(LOCAL_BUILD_DIR)/demo: $(OBJECTS_CPP) $(STATIC_LIBS)
	@echo "Linking $(@F)..."
	@$(CXX) $(CXXFLAGS) -o $@ $^

$(LOCAL_BUILD_DIR)/%.o: %.cpp
	@echo "Building $(@F)..."
	@mkdir -p $(@D)
	@$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	@rm -rf $(LOCAL_BUILD_DIR)/*