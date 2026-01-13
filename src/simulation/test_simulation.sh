#!/bin/bash
# test_simulation.sh
# Quick test script to verify the simulation is working

echo "========================================="
echo "Multi-Process Cache Simulation Test"
echo "========================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    if [ $1 -eq 0 ]; then
        echo -e "${GREEN}✓ $2${NC}"
    else
        echo -e "${RED}✗ $2${NC}"
    fi
}

# Function to print info
print_info() {
    echo -e "${YELLOW}→ $1${NC}"
}   

# Replacement for nc using built-in Bash networking
send_command() {
    local cmd=$1
    # Try to send command and read response via device file
    # Format: exec {fd}<>/dev/tcp/host/port
    exec 3<>/dev/tcp/localhost/5099
    echo -e "$cmd" >&3
    read -t 2 response <&3
    exec 3>&- # Close file descriptor
    echo "$response"
}

# Step 1: Check if executables exist
print_info "Checking if executables are built..."
if [ ! -f "./cache_node_server.exe" ]; then
    echo -e "${RED}Error: cache_node_server not found. Run 'make' first.${NC}"
    exit 1
fi

if [ ! -f "./simulation_controller.exe" ]; then
    echo -e "${RED}Error: simulation_controller not found. Run 'make' first.${NC}"
    exit 1
fi

print_status 0 "Executables found"
echo ""

# Step 2: Test single cache node
print_info "Testing single cache node..."

# Start a node in background
./cache_node_server test_node 5099 100 LRU > /tmp/test_node.log 2>&1 &
NODE_PID=$!

sleep 2

# Check if PID exists (Windows-compatible check)
if ps -p $NODE_PID > /dev/null; then
    print_status 0 "Node started (PID: $NODE_PID)"
    
    # Test PING
    print_info "Testing PING command..."
    RESPONSE=$(send_command "PING")
    if [[ "$RESPONSE" == *"PONG"* ]]; then
        print_status 0 "PING successful"
    else
        print_status 1 "PING failed (Response: $RESPONSE)"
    fi
    
    # Test SET
    print_info "Testing SET command..."
    RESPONSE=$(send_command "SET testkey testvalue")
    if [[ "$RESPONSE" == *"OK"* ]]; then
        print_status 0 "SET successful"
    else
        print_status 1 "SET failed (Response: $RESPONSE)"
    fi
    
    # Test GET
    print_info "Testing GET command..."
    RESPONSE=$(send_command "GET testkey")
    if [[ "$RESPONSE" == VALUE:* ]]; then
        print_status 0 "GET successful (Response: $RESPONSE)"
    else
        print_status 1 "GET failed (Response: $RESPONSE)"
    fi
    
    # Test INFO
    print_info "Testing INFO command..."
    RESPONSE=$(send_command "INFO")
    if [[ "$RESPONSE" == *"NODE:test_node"* ]]; then
        print_status 0 "INFO successful"
    else
        print_status 1 "INFO failed"
    fi
    
    # Kill the test node
    print_info "Stopping test node..."
    # Using taskkill for Windows native or kill for Git Bash
    kill $NODE_PID 2>/dev/null
    print_status 0 "Node stopped"
else
    print_status 1 "Failed to start node (PID $NODE_PID not found)"
fi



echo ""

# Step 3: Test simulation controller (quick test)
print_info "Testing simulation controller (10 second test)..."

# Create a minimal test scenario
timeout 15 ./simulation_controller ./cache_node_server 1 > /tmp/simulation_test.log 2>&1 &
SIM_PID=$!

sleep 12

# Check if simulation ran
if [ -f /tmp/simulation_test.log ]; then
    if grep -q "Started node" /tmp/simulation_test.log; then
        print_status 0 "Simulation controller working"
        
        # Show summary
        echo ""
        print_info "Simulation output summary:"
        grep -E "(Started node|Metrics|Total Operations|Successful)" /tmp/simulation_test.log | head -10
    else
        print_status 1 "Simulation controller may have issues"
        echo ""
        echo "Log output:"
        cat /tmp/simulation_test.log
    fi
else
    print_status 1 "Simulation did not produce output"
fi

# Cleanup any remaining processes
print_info "Cleaning up processes..."
pkill -9 cache_node_server 2>/dev/null
print_status 0 "Cleanup complete"

echo ""
echo "========================================="
echo "Test Complete!"
echo "========================================="
echo ""
echo "If all tests passed, you can run full simulations:"
echo "  make run-scenario1    # Normal operations"
echo "  make run-scenario2    # Node failure"
echo "  make run-scenario3    # Cascading failures"
echo "  make run-scenario4    # Burst traffic"
echo "  make run-all          # All scenarios"
echo ""