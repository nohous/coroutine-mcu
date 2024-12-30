# Compiler settings
#CXX = g++
CXX = clang++
CXXFLAGS = -Wall -Wextra -std=c++20 -I../../coronimo/include -I../../etl/include\
	-Wno-unused-variable\
	-Wno-unused-but-set-variable\
	-Wno-unused-parameter\
	-Wno-missing-braces\
	-ftemplate-backtrace-limit=0\
	-fdiagnostics-show-template-tree
LDFLAGS =

# Directories
SRC_DIR = .
BUILD_DIR = build

# Source files
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# Target executable
TARGET = basic-sample

# Default target
all: $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(OBJS:.o=.d)

$(BUILD_DIR)/$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean