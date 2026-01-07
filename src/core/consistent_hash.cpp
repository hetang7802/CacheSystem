#include "consistent_hash.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cstdint>

using namespace std;

// Simple hash function using sum of character values and shifts
// todo : use MD5 or MurmurHash3
uint64_t ConsistentHashRing::customHash(const string& input) const {
    uint64_t h = 0;
    const uint64_t prime = 31;
    
    for (size_t i = 0; i < input.length(); ++i) {
        h = h * prime + static_cast<unsigned char>(input[i]);
    }
    
    // Mix bits to improve distribution
    h ^= (h >> 33);
    h *= 0xff51afd7ed558ccdULL;
    h ^= (h >> 33);
    
    return h;
}

string ConsistentHashRing::getVirtualNodeId(const string& nodeId, int replicaIndex) const {
    return nodeId + ":" + to_string(replicaIndex);
}

ConsistentHashRing::ConsistentHashRing(int numVirtualNodes)
    : virtualNodesPerPhysicalNode(numVirtualNodes) {}

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

shared_ptr<Node> ConsistentHashRing::findNode(const string& key) const {
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
    string parentNodeId = it->second->id;
    
    // Start from next position to find replicas (skip primary)
    ++it;
    if (it == ring.end()) {
        it = ring.begin();
    }
    
    // Collect N unique replica nodes (different from primary)
    while (replicas.size() < min(nodes.size()-1, n)) {
            
        // Check if same as parent
        bool isTaken = it->second->id==parentNodeId;
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
