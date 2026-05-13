PKGS = ncursesw

# Compiler settings
CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra -g $(shell pkg-config --cflags $(PKGS))
LDFLAGS := $(shell pkg-config --libs $(PKGS))

# Target binary name
TARGET = mapviewer.out

# Source files
SRC = main.cpp helpers.cpp parser.cpp ui.cpp

# Default target
all: $(TARGET)

# Link the object file into the final executable
$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(TARGET) $(SRC)

# Clean up generated files
clean:
	rm -f $(TARGET)

.PHONY: all clean
