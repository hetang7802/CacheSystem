#include "cache.h"
#include "command_parser.h"
#include "distributed_cache.h"
#include <iostream>
#include <iomanip>
#include <memory>

using namespace std;

void printHelp() {
    cout << "\n=== Cache System - Phase 1, 2, 3 & 4 ===" << endl;
    cout << "Local Cache Commands:" << endl;
    cout << "  SET key value [ttl]      - Set a key-value pair" << endl;
    cout << "  GET key                  - Get value by key" << endl;
    cout << "  DEL key                  - Delete a key" << endl;
    cout << "  EXISTS key               - Check if key exists" << endl;
    cout << "  SIZE                     - Get number of keys" << endl;
    cout << "  CLEANUP                  - Remove expired keys" << endl;
    cout << "  CLEAR                    - Clear all keys" << endl;
    cout << "  CONFIG policy capacity   - Configure eviction (LRU/LFU/NONE)" << endl;
    cout << "  STATUS                   - Show cache status" << endl;
    cout << "\nDistributed Cache Commands (Phase 3 & 4):" << endl;
    cout << "  CLUSTER                  - Switch to distributed mode" << endl;
    cout << "  ADDNODE nodeId           - Add node to cluster" << endl;
    cout << "  REMOVENODE nodeId        - Remove node from cluster" << endl;
    cout << "  FAILNODE nodeId          - Simulate node failure" << endl;
    cout << "  FAILEDNODES              - List all failed nodes" << endl;
    cout << "  FINDNODE key             - Find which node has a key" << endl;
    cout << "  REPLICAS key [count]     - Find replica nodes (with data replication)" << endl;
    cout << "  NODES                    - List all nodes in cluster" << endl;
    cout << "  CLUSTERINFO              - Show cluster statistics" << endl;
    cout << "\nGeneral:" << endl;
    cout << "  HELP                     - Show this help message" << endl;
    cout << "  EXIT                     - Exit the program" << endl;
    cout << "\nExamples:" << endl;
    cout << "  SET name John" << endl;
    cout << "  CONFIG LRU 100" << endl;
    cout << "  CLUSTER" << endl;
    cout << "  ADDNODE node-1" << endl;
    cout << "  ADDNODE node-2" << endl;
    cout << "  SET user:123 alice" << endl;
    cout << "  FINDNODE user:123" << endl;
    cout << "  REPLICAS user:123 3" << endl;
    cout << "  FAILNODE node-1" << endl;
    cout << "  GET user:123" << endl;
    cout << "========================================\n" << endl;
}

int main() {
    unique_ptr<Cache> localCache = make_unique<Cache>();
    unique_ptr<DistributedCache> distributedCache;
    bool isDistributed = false;
    string input;

    cout << "in-memory Cache System (Phase 1, 2, 3 & 4)" << endl;
    cout << "Type 'HELP' for available commands" << endl;
    cout << "Mode: LOCAL" << endl << endl;

    while (true) {
        cout << "\ncache> ";
        getline(cin, input);

        if (input.empty()) continue;

        // Check for exit
        if (input == "EXIT" || input == "exit") {
            cout << "Goodbye!" << endl;
            break;
        }

        // Parse command
        auto cmd = CommandParser::parse(input);
        if (!cmd) {
            cout << "ERROR: Invalid command. Type 'HELP' for help." << endl;
            continue;
        }

        // Handle mode switching
        if (cmd->operation == "CLUSTER") {
            isDistributed = true;
            distributedCache = make_unique<DistributedCache>(150, 0, Cache::EvictionType::NONE);
            cout << "OK - Switched to distributed mode" << endl;
            cout << "Mode: DISTRIBUTED" << endl;
            continue;
        }

        // Execute local cache commands
        if (!isDistributed) {
            if (cmd->operation == "SET") {
                string key = cmd->args[0];
                string value = cmd->args[1];
                
                if (cmd->args.size() == 3) {
                    try {
                        int ttl = stoi(cmd->args[2]);
                        bool success = localCache->setWithTtl(key, value, ttl);
                        if (success) {
                            cout << "OK - Set " << key << " with TTL " << ttl << "s" << endl;
                        } else {
                            cout << "ERROR - Cache is full" << endl;
                        }
                    } catch (const exception& e) {
                        cout << "ERROR: Invalid TTL value" << endl;
                    }
                } else {
                    bool success = localCache->set(key, value);
                    if (success) {
                        cout << "OK - Set " << key << endl;
                    } else {
                        cout << "ERROR - Cache is full" << endl;
                    }
                }
            }
            else if (cmd->operation == "GET") {
                string key = cmd->args[0];
                auto value = localCache->get(key);
                
                if (value) {
                    cout << "\"" << value.value() << "\"" << endl;
                } else {
                    cout << "(nil)" << endl;
                }
            }
            else if (cmd->operation == "DEL") {
                string key = cmd->args[0];
                if (localCache->del(key)) {
                    cout << "(integer) 1" << endl;
                } else {
                    cout << "(integer) 0" << endl;
                }
            }
            else if (cmd->operation == "EXISTS") {
                string key = cmd->args[0];
                if (localCache->exists(key)) {
                    cout << "(integer) 1" << endl;
                } else {
                    cout << "(integer) 0" << endl;
                }
            }
            else if (cmd->operation == "SIZE") {
                cout << "(integer) " << localCache->size() << endl;
            }
            else if (cmd->operation == "CLEANUP") {
                size_t removed = localCache->cleanupExpired();
                cout << "(integer) " << removed << " - Cleaned up " << removed 
                      << " expired keys" << endl;
            }
            else if (cmd->operation == "CLEAR") {
                localCache->clear();
                cout << "OK - Cache cleared" << endl;
            }
            else if (cmd->operation == "STATUS") {
                cout << "Cache Status:" << endl;
                cout << "  Keys: " << localCache->size() << endl;
                cout << "  Capacity: " << (localCache->capacity() == 0 ? "Unlimited" : to_string(localCache->capacity())) << endl;
                cout << "  Eviction Policy: " << localCache->evictionPolicy() << endl;
                cout << "  Evicted: " << localCache->evictionCount() << " keys" << endl;
            }
            else if (cmd->operation == "CONFIG") {
                if (cmd->args.size() < 2) {
                    cout << "ERROR: CONFIG requires policy and capacity" << endl;
                    cout << "Usage: CONFIG LRU 100" << endl;
                } else {
                    string policy = cmd->args[0];
                    try {
                        size_t capacity = stoul(cmd->args[1]);
                        
                        Cache::EvictionType evictionType;
                        if (policy == "LRU") {
                            evictionType = Cache::EvictionType::LRU;
                        } else if (policy == "LFU") {
                            evictionType = Cache::EvictionType::LFU;
                        } else if (policy == "NONE") {
                            evictionType = Cache::EvictionType::NONE;
                        } else {
                            cout << "ERROR: Unknown policy. Use LRU, LFU, or NONE" << endl;
                            continue;
                        }
                        
                        localCache = make_unique<Cache>(evictionType, capacity);
                        cout << "OK - Cache configured: " << policy << " with capacity " << capacity << endl;
                    } catch (const exception& e) {
                        cout << "ERROR: Invalid capacity value" << endl;
                    }
                }
            }
            else if (cmd->operation == "HELP") {
                printHelp();
            }
        }
        // Execute distributed cache commands
        else {
            if (cmd->operation == "SET") {
                string key = cmd->args[0];
                string value = cmd->args[1];
                
                if (cmd->args.size() == 3) {
                    try {
                        int ttl = stoi(cmd->args[2]);
                        bool success = distributedCache->setWithTtl(key, value, ttl);
                        if (success) {
                            cout << "OK - Set " << key << " with TTL " << ttl << "s" << endl;
                        } else {
                            cout << "ERROR - Failed to set" << endl;
                        }
                    } catch (const exception& e) {
                        cout << "ERROR: Invalid TTL value" << endl;
                    }
                } else {
                    bool success = distributedCache->set(key, value);
                    if (success) {
                        cout << "OK - Set " << key << endl;
                    } else {
                        cout << "ERROR - Failed to set" << endl;
                    }
                }
            }
            else if (cmd->operation == "GET") {
                string key = cmd->args[0];
                auto value = distributedCache->get(key);
                
                if (value) {
                    cout << "\"" << value.value() << "\"" << endl;
                } else {
                    cout << "(nil)" << endl;
                }
            }
            else if (cmd->operation == "DEL") {
                string key = cmd->args[0];
                if (distributedCache->del(key)) {
                    cout << "(integer) 1" << endl;
                } else {
                    cout << "(integer) 0" << endl;
                }
            }
            else if (cmd->operation == "EXISTS") {
                string key = cmd->args[0];
                if (distributedCache->exists(key)) {
                    cout << "(integer) 1" << endl;
                } else {
                    cout << "(integer) 0" << endl;
                }
            }
            else if (cmd->operation == "SIZE") {
                cout << "(integer) " << distributedCache->size() << endl;
            }
            else if (cmd->operation == "ADDNODE") {
                string nodeId = cmd->args[0];
                if (distributedCache->addNode(nodeId)) {
                    cout << "OK - Node '" << nodeId << "' added to cluster" << endl;
                } else {
                    cout << "ERROR - Node already exists or failed" << endl;
                }
            }
            else if (cmd->operation == "REMOVENODE") {
                string nodeId = cmd->args[0];
                if (distributedCache->removeNode(nodeId)) {
                    cout << "OK - Node '" << nodeId << "' removed from cluster" << endl;
                } else {
                    cout << "ERROR - Node not found" << endl;
                }
            }
            else if (cmd->operation == "FAILNODE") {
                string nodeId = cmd->args[0];
                if (distributedCache->markNodeFailed(nodeId)) {
                    cout << "OK - Node '" << nodeId << "' marked as failed" << endl;
                    cout << "     Operations will now use replicas for keys on this node" << endl;
                } else {
                    cout << "ERROR - Node not found or already failed" << endl;
                }
            }
            else if (cmd->operation == "FAILEDNODES") {
                auto failedNodes = distributedCache->getFailedNodes();
                if (failedNodes.empty()) {
                    cout << "No failed nodes" << endl;
                } else {
                    cout << "Failed nodes (" << failedNodes.size() << "):" << endl;
                    for (size_t i = 0; i < failedNodes.size(); ++i) {
                        cout << "  " << (i + 1) << ". " << failedNodes[i] << endl;
                    }
                }
            }
            else if (cmd->operation == "FINDNODE") {
                string key = cmd->args[0];
                string node = distributedCache->findKeyNode(key);
                if (!node.empty()) {
                    cout << "Key '" << key << "' -> Node: " << node << endl;
                } else {
                    cout << "ERROR - No nodes in cluster" << endl;
                }
            }
            else if (cmd->operation == "REPLICAS") {
                string key = cmd->args[0];
                size_t count = 2;
                if (cmd->args.size() == 2) {
                    try {
                        count = stoul(cmd->args[1]);
                    } catch (const exception& e) {
                        cout << "ERROR: Invalid count" << endl;
                        continue;
                    }
                }
                
                auto replicas = distributedCache->getKeyReplicas(key, count);
                cout << "Replicas for '" << key << "': ";
                for (size_t i = 0; i < replicas.size(); ++i) {
                    cout << replicas[i];
                    if (i < replicas.size() - 1) cout << ", ";
                }
                cout << endl;
            }
            else if (cmd->operation == "NODES") {
                auto nodes = distributedCache->getAllNodes();
                cout << "Nodes in cluster (" << nodes.size() << "):" << endl;
                for (size_t i = 0; i < nodes.size(); ++i) {
                    cout << "  " << (i + 1) << ". " << nodes[i];
                    cout << " [" << distributedCache->getNodeSize(nodes[i]) << " keys]";
                    if (distributedCache->isNodeFailed(nodes[i])) {
                        cout << " [FAILED]";
                    }
                    cout << endl;
                }
            }
            else if (cmd->operation == "CLUSTERINFO") {
                auto stats = distributedCache->getStats();
                cout << "Cluster Information:" << endl;
                cout << "  Nodes: " << stats.totalNodes << endl;
                cout << "  Total Keys: " << stats.totalKeys << endl;
                cout << "  Replication Factor: " << stats.replicationFactor << " (primary + " << (stats.replicationFactor - 1) << " replicas)" << endl;
                cout << "  Virtual Nodes per Physical: " << stats.virtualNodesPerPhysicalNode << endl;
                cout << "  Capacity per Node: " << (stats.maxCapacityPerNode == 0 ? "Unlimited" : to_string(stats.maxCapacityPerNode)) << endl;
            }
            else if (cmd->operation == "CLEAR") {
                distributedCache->clear();
                cout << "OK - Cluster cleared" << endl;
            }
            else if (cmd->operation == "HELP") {
                printHelp();
            }
        }
    }

    return 0;
}