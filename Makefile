# Compiler
CXX = g++
CXXFLAGS = -Wall -g

# Build directory
BIN = bin

# Target executable
TARGET = $(BIN)/test

# Source and object files
SRC = src/Logger.cpp src/main.cpp
OBJ = $(BIN)/Logger.o $(BIN)/main.o

# Default target
all: $(TARGET)

# Link
$(TARGET): $(OBJ)
	$(CXX) -o $@ $(OBJ)

# Compile Logger.cpp
$(BIN)/Logger.o: src/Logger.cpp src/Logger.h | $(BIN)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile main.cpp
$(BIN)/main.o: src/main.cpp src/Logger.h | $(BIN)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Ensure bin directory exists
$(BIN):
	mkdir -p $(BIN)

# Clean
clean:
	rm -f $(BIN)/*.o $(TARGET)
