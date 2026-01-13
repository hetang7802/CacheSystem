#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <atomic>
#include <vector>
#include <memory>

// Windows Networking Headers
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "distributed_cache.h"

// Link with the Winsock library
#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif

using namespace std;

vector<string> parseInputLine(char *c){
    vector<string> ans ;
    int i = 0;
    while(c[i]){
        while(c[i]==' ')i++;
        ans.push_back("");
        while(c[i] && c[i]!=' '){
            ans.back().push_back(c[i]);
            i++;
        }
    }
    return ans;
}


class CacheNodeServer {
private:
    string nodeId;
    int port;
    SOCKET listenSocket;
    atomic<bool> running{false};
    unique_ptr<DistributedCache> cache;
    
    size_t maxCapacity;
    Cache::EvictionType evictionPolicy;

public:
    CacheNodeServer(const string& nodeId, int port, size_t maxCapacity, 
                    Cache::EvictionType policy)
        : nodeId(nodeId), port(port), maxCapacity(maxCapacity), 
          evictionPolicy(policy), listenSocket(INVALID_SOCKET) {
        
        cache = make_unique<DistributedCache>(evictionPolicy, maxCapacity);
    }

    ~CacheNodeServer() {
        stop();
    }

    bool start() {
        // 1. Initialize Winsock
        WSADATA wsaData;
        int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (iResult != 0) {
            cerr << "[" << nodeId << "] WSAStartup failed: " << iResult << endl;
            return false;
        }

        // 2. Create Socket
        listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listenSocket == INVALID_SOCKET) {
            cerr << "[" << nodeId << "] Socket creation failed: " << WSAGetLastError() << endl;
            WSACleanup();
            return false;
        }

        // 3. Set Socket Options (REUSEADDR is slightly different on Windows, but often useful)
        BOOL opt = TRUE;
        setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

        // 4. Bind
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons((u_short)port);

        if (bind(listenSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            cerr << "[" << nodeId << "] Bind failed: " << WSAGetLastError() << endl;
            closesocket(listenSocket);
            WSACleanup();
            return false;
        }

        // 5. Listen
        if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
            cerr << "[" << nodeId << "] Listen failed: " << WSAGetLastError() << endl;
            closesocket(listenSocket);
            WSACleanup();
            return false;
        }

        running = true;
        cout << "[" << nodeId << "] Server started on port " << port << " (Windows Winsock)" << endl;

        acceptLoop();
        return true;
    }

    void stop() {
        running = false;
        if (listenSocket != INVALID_SOCKET) {
            closesocket(listenSocket);
            listenSocket = INVALID_SOCKET;
        }
        WSACleanup(); // Clean up Winsock
    }

private:
    void acceptLoop() {
        while (running) {
            sockaddr_in clientAddr;
            int clientLen = sizeof(clientAddr);
            
            SOCKET clientSocket = accept(listenSocket, (struct sockaddr*)&clientAddr, &clientLen);
            
            if (clientSocket == INVALID_SOCKET) {
                if (running) {
                    cerr << "[" << nodeId << "] Error accepting connection: " << WSAGetLastError() << endl;
                }
                continue;
            }
            
            // Note: Windows threads work fine with standard C++11 thread
            thread(&CacheNodeServer::handleClient, this, clientSocket).detach();
        }
    }

    void handleClient(SOCKET clientSocket) {
        char buffer[4096];
        memset(buffer, 0, sizeof(buffer));
        
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytesRead > 0) {
            string command(buffer);
            string response = processCommand(command);
            send(clientSocket, response.c_str(), (int)response.length(), 0);
        }
        
        closesocket(clientSocket);
    }

    // Command processing logic remains identical as it's high-level C++
    string processCommand(const string& command) {
        istringstream iss(command);
        
        try {
            char* input;
            cin.getline(input, 4096);
            vector<string> parsedInput = parseInputLine(input);
            string operation;
            vector<string> args;
            for(int i=0;i<parsedInput.size();i++){
                if(i==0){
                    operation = parsedInput[i];
                }else{
                    args.push_back(parsedInput[i]);
                }
            }
            stringstream stream;
            if (operation == "SET") {
                string key, value ;
                if (args.size() == 3) {
                    try {
                        int ttl = stoi(args[2]);
                        bool success = cache->setWithTtl(key, value, ttl);
                        if (success) {
                            stream << "OK - Set " << key << " with TTL " << ttl << "s" << endl;
                        } else {
                            stream << "ERROR - Cache is full" << endl;
                        }
                    } catch (const exception& e) {
                        stream << "ERROR: Invalid TTL value" << endl;
                    }
                } else {
                    bool success = cache->set(key, value);
                    if (success) {
                        stream << "OK - Set " << key << endl;
                    } else {
                        stream << "ERROR - Cache is full" << endl;
                    }
                }
            }
            else if (operation == "GET") {
                string key = args[0];
                auto value = cache->get(key);
                
                if (value) {
                    stream << "\"" << value.value() << "\"" << endl;
                } else {
                    stream << "(nil)" << endl;
                }
            }
            else if (operation == "DEL") {
                string key = args[0];
                if (cache->del(key)) {
                    stream << "(integer) 1" << endl;
                } else {
                    stream << "(integer) 0" << endl;
                }
            }
            else if (operation == "EXISTS") {
                string key = args[0];
                if (cache->exists(key)) {
                    stream << "(integer) 1" << endl;
                } else {
                    stream << "(integer) 0" << endl;
                }
            }
            else if (operation == "SIZE") {
                stream << "(integer) " << cache->size() << endl;
            }
            // else if (operation == "CLEANUP") {
            //     size_t removed = cache->cleanupExpired();
            //     stream << "(integer) " << removed << " - Cleaned up " << removed 
            //           << " expired keys" << endl;
            // }
            else if (operation == "CLEAR") {
                cache->clear();
                stream << "OK - Cache cleared" << endl;
            }
            // else if (operation == "STATUS") {
            //     stream << "Cache Status:" << endl;
            //     stream << "  Keys: " << cache->size() << endl;
            //     stream << "  Capacity: " << (cache->capacity() == 0 ? "Unlimited" : to_string(cache->capacity())) << endl;
            //     stream << "  Eviction Policy: " << cache->evictionPolicy() << endl;
            //     stream << "  Evicted: " << cache->evictionCount() << " keys" << endl;
            // }
            // else if (operation == "CONFIG") {
            //     if (args.size() < 2) {
            //         stream << "ERROR: CONFIG requires policy and capacity" << endl;
            //         stream << "Usage: CONFIG LRU 100" << endl;
            //     } else {
            //         string policy = args[0];
            //         try {
            //             size_t capacity = stoul(args[1]);
                        
            //             Cache::EvictionType evictionType;
            //             if (policy == "LRU") {
            //                 evictionType = Cache::EvictionType::LRU;
            //             } else if (policy == "LFU") {
            //                 evictionType = Cache::EvictionType::LFU;
            //             } else if (policy == "NONE") {
            //                 evictionType = Cache::EvictionType::NONE;
            //             } else {
            //                 stream << "ERROR: Unknown policy. Use LRU, LFU, or NONE" << endl;
            //             }
                        
            //             cache = make_unique<DistributedCache>(evictionType, capacity);
            //             stream << "OK - Cache configured: " << policy << " with capacity " << capacity << endl;
            //         } catch (const exception& e) {
            //             stream << "ERROR: Invalid capacity value" << endl;
            //         }
            //     }
            // }
            else if (operation == "HELP") {
                return "NEED TO CONFIGURE HELP";
            }
            return stream.str();
        }
        catch (const exception& e) {
            return string("ERROR: ") + e.what();
        }
    }
};

// --- Signal Handling for Windows ---

CacheNodeServer* globalServer = nullptr;

// Windows Console Control Handler
BOOL WINAPI ConsoleHandler(DWORD ctrlType) {
    switch (ctrlType) {
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_BREAK_EVENT:
            cout << "\nShutting down cache node..." << endl;
            if (globalServer) globalServer->stop();
            return TRUE;
        default:
            return FALSE;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <node_id> <port> [capacity] [policy]" << endl;
        return 1;
    }

    string nodeId = argv[1];
    int port = stoi(argv[2]);
    size_t capacity = (argc > 3) ? stoul(argv[3]) : 1000;
    
    Cache::EvictionType policy = Cache::EvictionType::LRU;
    if (argc > 4 && string(argv[4]) == "LFU") policy = Cache::EvictionType::LFU;

    // Set Windows Console Handler
    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) {
        cerr << "Could not set control handler" << endl;
        return 1;
    }

    CacheNodeServer server(nodeId, port, capacity, policy);
    globalServer = &server;

    server.start();

    return 0;
}