# --- Compiler Settings ---
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread -O2
# Look for headers in the include folder
INCLUDES = -I./include

# --- Directory Paths ---
SRC_CORE = ./src/core
SRC_SIM  = ./src/simulation
# Folder for final compiled executables
BIN_DIR  = ./bin

# --- Source Files ---
# Core logic components
CORE_SOURCES = $(SRC_CORE)/cache.cpp \
               $(SRC_CORE)/command_parser.cpp \
               $(SRC_CORE)/consistent_hash.cpp \
               $(SRC_CORE)/distributed_cache.cpp \
               $(SRC_CORE)/eviction.cpp

# Main application components
SERVER_SOURCE     = $(SRC_SIM)/cache_node_server.cpp
CONTROLLER_SOURCE = $(SRC_SIM)/simulation_controller.cpp

# --- Object Files ---
CORE_OBJS       = $(CORE_SOURCES:.cpp=.o)
SERVER_OBJ      = $(SRC_SIM)/cache_node_server.o
CONTROLLER_OBJ  = $(SRC_SIM)/simulation_controller.o

# --- Targets ---

all: directories $(BIN_DIR)/cache_node_server $(BIN_DIR)/simulation_controller

# Create the bin directory for the final executables
directories:
	@mkdir -p $(BIN_DIR)

# --- Executable Rules ---

# Linked with all core logic object files
# --- Executable Rules ---

# Add -lws2_32 to the end of the line
$(BIN_DIR)/cache_node_server: $(SERVER_OBJ) $(CORE_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(INCLUDES) -lws2_32

$(BIN_DIR)/simulation_controller: $(CONTROLLER_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(INCLUDES) -lws2_32
# --- Compilation Rules ---

# Compile Core files
$(SRC_CORE)/%.o: $(SRC_CORE)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Compile Simulation files
$(SRC_SIM)/%.o: $(SRC_SIM)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# --- Simulation Run Commands ---

run-scenario1: all
	$(BIN_DIR)/simulation_controller $(BIN_DIR)/cache_node_server 1

run-all: all
	$(BIN_DIR)/simulation_controller $(BIN_DIR)/cache_node_server all

# --- Cleanup ---

clean:
	rm -f $(SRC_CORE)/*.o $(SRC_SIM)/*.o
	rm -rf $(BIN_DIR)

.PHONY: all clean directories run-scenario1 run-all