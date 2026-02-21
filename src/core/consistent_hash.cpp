#include "consistent_hash.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cstdint>
#include <unordered_set>

using namespace std;


uint64_t ConsistentHashRing::customHash(const string& input) const {
    return hashFunc(input);
}

string ConsistentHashRing::getVirtualNodeId(const string& nodeId, int replicaIndex) const {
    return nodeId + ":" + to_string(replicaIndex);
}

ConsistentHashRing::ConsistentHashRing(int numVirtualNodes)
    : virtualNodesPerPhysicalNode(numVirtualNodes) {
        hashFunc = hash<string>();
    }

bool ConsistentHashRing::addNode(const Node& node) {
    // Check if node already exists
    if (hasNode(node.id)) {
        return false;
    }
    
    // Create and store the node
    auto newNode = make_shared<Node>(node);
    nodes[node.id] = newNode;
    
    // Add virtual nodes to the ring
    addVirtualNodes(newNode);
    
    return true;
}

bool ConsistentHashRing::removeNode(const string& nodeId) {
    if (!hasNode(nodeId)) {
        return false;
    }
    
    // Remove virtual nodes from the ring
    removeVirtualNodes(nodeId);
    
    // Remove the node
    nodes.erase(nodeId);
    
    return true;
}

void ConsistentHashRing::addVirtualNodes(const shared_ptr<Node>& node) {
    for (int i = 0; i < virtualNodesPerPhysicalNode; ++i) {
        string virtualNodeId = getVirtualNodeId(node->id, i);
        uint64_t hashValue = customHash(virtualNodeId);
        ring[hashValue] = node;
    }
}

void ConsistentHashRing::removeVirtualNodes(const string& nodeId) {
    for (int i = 0; i < virtualNodesPerPhysicalNode; ++i) {
        string virtualNodeId = getVirtualNodeId(nodeId, i);
        uint64_t hashValue = customHash(virtualNodeId);
        ring.erase(hashValue);
    }
}

shared_ptr<Node> ConsistentHashRing::findPrimaryNode(const string& key) const {
    if (ring.empty()) {
        return nullptr;
    }
    
    uint64_t keyHash = customHash(key);
    
    // Find the first node with hash >= keyHash
    auto it = ring.lower_bound(keyHash);
    
    // If not found, wrap around to the first node in the ring
    if (it == ring.end()) {
        it = ring.begin();
    }
    
    return it->second;
}

// OVERLOAD: findNode that skips failed nodes
map<uint64_t, shared_ptr<Node>>::const_iterator ConsistentHashRing::findPrimaryNodeIt(
    const string& key, 
    const unordered_set<string>& failedNodes) const {
    
    if (ring.empty()) {
        return ring.end();
    }
    
    uint64_t keyHash = customHash(key);
    
    // Find the first node with hash >= keyHash
    auto it = ring.lower_bound(keyHash);
    
    // If not found, wrap around to the first node in the ring
    if (it == ring.end()) {
        it = ring.begin();
    }
    
    // Track starting position to detect if we've checked all nodes
    auto startIt = it;
    bool wrappedAround = false;
    
    // Find first healthy node
    while (true) {
        // Check if we've wrapped around completely
        if (wrappedAround && it == startIt) {
            // We've checked all nodes, none are healthy
            return ring.end();
        }
        
        // Check if this node is healthy
        if (failedNodes.find(it->second->id) == failedNodes.end()) {
            // Found a healthy node!
            return it;
        }
        
        // Move to next node
        ++it;
        if (it == ring.end()) {
            it = ring.begin();
            wrappedAround = true;
        }
    }
    
    return ring.end();  // Should never reach here
}



// OVERLOAD: findNode that skips failed nodes
shared_ptr<Node> ConsistentHashRing::findPrimaryNode(
    const string& key, 
    const unordered_set<string>& failedNodes) const {
        
    auto it = findPrimaryNodeIt(key, failedNodes);
    if(it==ring.end()){
        return nullptr;
    }
    return it->second;
}

vector<shared_ptr<Node>> ConsistentHashRing::getReplicaNodes(const string& key, size_t n) const {
    vector<shared_ptr<Node>> replicas;
    
    if (ring.empty()) {
        return replicas;
    }
    
    uint64_t keyHash = customHash(key);
    auto it = ring.lower_bound(keyHash);
    
    if (it == ring.end()) {
        it = ring.begin();
    }
    
    // Get the primary node and initialize lastNodeId to it
    // This ensures we skip the primary and only collect N DIFFERENT replicas
    string primaryNodeId = it->second->id;
    
    // Start from next position to find replicas (skip primary)
    ++it;
    if (it == ring.end()) {
        it = ring.begin();
    }
    
    // Collect N unique replica nodes (different from primary)
    while (replicas.size() < min(nodes.size()-1, n)) {
            
        // Check if same as parent
        bool isTaken = it->second->id==primaryNodeId;
        // Check if already in replicas
        if(!isTaken){
            for (const auto& replica : replicas) {
                if (replica->id == it->second->id) {
                    isTaken = true;
                    break;
                }
            }
        }
        
        if (!isTaken) {
            replicas.push_back(it->second);
        }
        
        ++it;
        if (it == ring.end()) {
            it = ring.begin();
        }
    }
    
    return replicas;
}

// OVERLOAD: getReplicaNodes with failed nodes filter
vector<shared_ptr<Node>> ConsistentHashRing::getReplicaNodes(
    const string& key, 
    size_t n, 
    const unordered_set<string>& failedNodes) const {
    
    vector<shared_ptr<Node>> replicas;
    
    auto it = findPrimaryNodeIt(key, failedNodes);

    // Get the primary node
    string primaryNodeId = it->second->id;
    // cout << "[DEBUG] << primary is " << primaryNodeId << endl;

    // Start from next position to find replicas (skip primary)
    ++it;
    if (it == ring.end()) {
        it = ring.begin();
    }
    
    // Track starting position to avoid infinite loop
    auto startIt = it;
    bool wrappedAround = false;
    
    // Collect N unique healthy replica nodes (different from primary and not failed)
    while (replicas.size() < min(nodes.size()-1, n)) {
        
        // Check if we've wrapped around completely
        if (wrappedAround && it == startIt) {
            break;  // We've checked all nodes, can't find more replicas
        }
        
        string currentNodeId = it->second->id;
        
        // Check if this node should be skipped
        bool shouldSkip = false;
        
        // Skip if same as primary
        if (currentNodeId == primaryNodeId) {
            shouldSkip = true;
        }
        
        // Skip if failed
        if (!shouldSkip && failedNodes.find(currentNodeId) != failedNodes.end()) {
            shouldSkip = true;
        }
        
        // Skip if already in replicas
        if (!shouldSkip) {
            for (const auto& replica : replicas) {
                if (replica->id == currentNodeId) {
                    shouldSkip = true;
                    break;
                }
            }
        }
        
        // Add if not skipped
        if (!shouldSkip) {
            // cout << "[DEBUG] : taking " << it->second->id << endl;
            replicas.push_back(it->second);
        }
        
        // Move to next node
        ++it;
        if (it == ring.end()) {
            it = ring.begin();
            wrappedAround = true;
        }
    }
    
    return replicas;
}

vector<string> ConsistentHashRing::getAllNodes() const {
    vector<string> nodeIds;
    for (const auto& [id, node] : nodes) {
        nodeIds.push_back(id);
    }
    return nodeIds;
}

bool ConsistentHashRing::hasNode(const string& nodeId) const {
    return nodes.find(nodeId) != nodes.end();
}

void ConsistentHashRing::clear() {
    ring.clear();
    nodes.clear();
}