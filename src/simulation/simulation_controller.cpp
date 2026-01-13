#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <thread>
#include <chrono>
#include <random>
#include <sstream>

// Windows Specific Headers
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

// Link with Ws2_32.lib
#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif

using namespace std;
using namespace chrono;

struct NodeInfo {
    string nodeId;
    int port;
    HANDLE hProcess;      // Windows Process Handle
    DWORD processId;
    bool running;

    NodeInfo(const string& id, int p)
        : nodeId(id), port(p), hProcess(NULL), processId(0), running(false) {}
};

class SimulationController {
private:
    vector<NodeInfo> nodes;
    map<string, int> nodeIndexMap;
    string cacheNodeExecutable;

    size_t totalOperations = 0;
    size_t successfulOps = 0;
    size_t failedOps = 0;

    random_device rd;
    mt19937 gen;

    // Winsock initialization
    void initWinsock() {
        WSADATA wsaData;
        int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (iResult != 0) {
            cerr << "WSAStartup failed: " << iResult << endl;
        }
    }

public:
    SimulationController(const string& executable)
        : cacheNodeExecutable(executable), gen(rd()) {
        initWinsock();
    }

    ~SimulationController() {
        stopAllNodes();
        WSACleanup();
    }

    bool startNode(const string& nodeId, int port, size_t capacity = 1000) {
        if (nodeIndexMap.find(nodeId) != nodeIndexMap.end()) {
            cout << "Node " << nodeId << " already exists" << endl;
            return false;
        }

        // 1. Wrap the executable in quotes to handle paths with spaces
        // 2. Build the full command line
        stringstream ss;
        ss << cacheNodeExecutable << " " << nodeId << " " << port << " " << capacity << " LRU";
        string cmdLine = ss.str();

        // 3. Create a modifiable buffer (Required for CreateProcessA)
        vector<char> cmdBuffer(cmdLine.begin(), cmdLine.end());
        cmdBuffer.push_back('\0'); 

        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        // 4. Call CreateProcessA using the modifiable buffer
        cout << "Debug: Executing -> " << cmdLine << endl;
        if (!CreateProcessA(
            NULL,               // Application Name
            cmdBuffer.data(),   // Command Line (Modifiable)
            NULL,               // Process Attributes
            NULL,               // Thread Attributes
            FALSE,              // Inherit Handles
            0,                  // Creation Flags
            NULL,               // Environment
            NULL,               // Current Directory
            &si,                // Startup Info
            &pi                 // Process Information
        )) {
            DWORD error = GetLastError();
            cerr << "[ERROR] CreateProcess failed for " << nodeId << ". Error Code: " << error << endl;
            if (error == 2) cerr << "Tip: File not found. Check your executable path." << endl;
            if (error == 5) cerr << "Tip: Access Denied. Check permissions." << endl;
            return false;
        }

        NodeInfo node(nodeId, port);
        node.hProcess = pi.hProcess;
        node.processId = pi.dwProcessId;
        node.running = true;

        CloseHandle(pi.hThread);

        nodes.push_back(node);
        nodeIndexMap[nodeId] = (int)nodes.size() - 1;

        this_thread::sleep_for(milliseconds(500));
        cout << "[CONTROLLER] Started node " << nodeId << " (PID: " << node.processId << ")" << endl;

        return true;
    }

    bool stopNode(const string& nodeId) {
        auto it = nodeIndexMap.find(nodeId);
        if (it == nodeIndexMap.end()) return false;

        NodeInfo& node = nodes[it->second];
        if (!node.running) return false;

        // In Windows, there is no direct SIGTERM equivalent via API. 
        // We use TerminateProcess for simplicity in this simulation.
        TerminateProcess(node.hProcess, 0);
        WaitForSingleObject(node.hProcess, INFINITE);
        CloseHandle(node.hProcess);

        node.running = false;
        node.hProcess = NULL;
        cout << "[CONTROLLER] Stopped node " << nodeId << endl;
        return true;
    }

    void stopAllNodes() {
        for (auto& node : nodes) {
            if (node.running) {
                TerminateProcess(node.hProcess, 0);
                WaitForSingleObject(node.hProcess, 500);
                CloseHandle(node.hProcess);
                node.running = false;
            }
        }
    }

    bool killNode(const string& nodeId) {
        return stopNode(nodeId); // On Windows, TerminateProcess is already forceful
    }

    string sendCommand(const string& nodeId, const string& command) {
        auto it = nodeIndexMap.find(nodeId);
        if (it == nodeIndexMap.end()) return "ERROR: Node not found";

        NodeInfo& node = nodes[it->second];
        if (!node.running) return "ERROR: Node not running";

        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) return "ERROR: Socket creation failed";

        // Set timeouts
        DWORD timeout = 2000; // 2 seconds
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons((u_short)node.port);
        inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

        if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            closesocket(sock);
            return "ERROR: Connection failed";
        }

        send(sock, command.c_str(), (int)command.length(), 0);

        char buffer[4096];
        memset(buffer, 0, sizeof(buffer));
        int bytesRead = recv(sock, buffer, sizeof(buffer) - 1, 0);

        closesocket(sock);

        if (bytesRead <= 0) return "ERROR: No response";
        return string(buffer);
    }

    // ... [Workload Generation and Scenarios remain logically identical to original] ...
    // (Ensure you copy runUniformWorkload, runBurstWorkload, and all Scenarios here)

    void runUniformWorkload(int durationSec, int opsPerSecond) {
        cout << "\n=== Running Uniform Workload (Windows) ===" << endl;
        auto startTime = steady_clock::now();
        auto endTime = startTime + seconds(durationSec);
        totalOperations = 0; successfulOps = 0; failedOps = 0;

        while (steady_clock::now() < endTime) {
            auto loopStart = steady_clock::now();
            for (int i = 0; i < opsPerSecond; ++i) executeRandomOperation();
            auto loopEnd = steady_clock::now();
            auto elapsed = duration_cast<milliseconds>(loopEnd - loopStart);
            auto sleepTime = milliseconds(1000) - elapsed;
            if (sleepTime.count() > 0) this_thread::sleep_for(sleepTime);
        }
        printMetrics();
    }

    // ... [Add remaining private helper methods from your original code] ...
    void executeRandomOperation() {
        totalOperations++;
        if (nodes.empty()) { failedOps++; return; }
        uniform_int_distribution<> nodeDist(0, (int)nodes.size() - 1);
        int nodeIdx = nodeDist(gen);
        if (!nodes[nodeIdx].running) { failedOps++; return; }
        
        uniform_int_distribution<> opDist(1, 100);
        int opChoice = opDist(gen);
        uniform_int_distribution<> keyDist(1, 1000);
        string key = "key_" + to_string(keyDist(gen));

        string command = (opChoice <= 70) ? "GET " + key : 
                         (opChoice <= 90) ? "SET " + key + " val" : "DEL " + key;

        string response = sendCommand(nodes[nodeIdx].nodeId, command);
        cout << command << endl;
        cout << response << endl;
        if (response.find("ERROR") == string::npos) successfulOps++; else failedOps++;
    }

    void printMetrics() {
        cout << "\n--- Metrics ---" << endl;
        cout << "Total Operations: " << totalOperations << endl;
        if (totalOperations > 0) {
            cout << "Successful: " << successfulOps << " (" << (100.0 * successfulOps / totalOperations) << "%)" << endl;
            cout << "Failed: " << failedOps << " (" << (100.0 * failedOps / totalOperations) << "%)" << endl;
        }
    }

    void runScenario1_NormalOperations() {
        cout << "\nScenario 1: Normal Operations" << endl;
        startNode("node1", 5001);
        startNode("node2", 5002);
        this_thread::sleep_for(seconds(2));
        runUniformWorkload(10, 50);
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <cache_node_exe_path> [scenario]" << endl;
        return 1;
    }

    string executable = argv[1];
    string scenario = (argc > 2) ? argv[2] : "1";

    SimulationController controller(executable);
    
    if (scenario == "1") controller.runScenario1_NormalOperations();
    // ... [Add other scenario triggers] ...

    cout << "\n=== Simulation Complete ===" << endl;
    return 0;
}