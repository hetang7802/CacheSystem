#include "distributed_cache.h"
#include <mutex>
#include <iostream>

using namespace std;

DistributedCache::DistributedCache(int numVirtualNodes,
                                   size_t maxCapacityPerNode,
                                   Cache::EvictionType evictionPolicy,
                                   size_t replicationFactor)
    : hashRing(numVirtualNodes),
      maxCapacityPerNode(maxCapacityPerNode),
      evictionPolicy(evictionPolicy),
      replicationFactor(replicationFactor) {}

bool DistributedCache::addNode(const string& nodeId) {
    unique_lock<shared_mutex> lock(mutex);
    
    Node node(nodeId);
    return hashRing.addNode(node);
}

bool DistributedCache::removeNode(const string& nodeId) {
    unique_lock<shared_mutex> lock(mutex);
    
    // Get all keys from the node being removed
    auto nodeIt = nodeCache.find(nodeId);
    unordered_map<string, Value> dataToRedistribute;
    
    if (nodeIt != nodeCache.end()) {
        dataToRedistribute = nodeIt->second->getAllData();
    }
    
    // Remove from hash ring
    if (!hashRing.removeNode(nodeId)) {
        return false;
    }
    
    // Remove cache instance from local map
    nodeCache.erase(nodeId);
    
    for(auto &itr : dataToRedistribute){
        string key = itr.first;
        Value value = itr.second;
        
        // Skip expired keys
        if (value.isExpired()) {
            continue;
        }
        
        auto node = hashRing.findNode(key);
        if (!node) {
            cout << "WARNING : node removed but no node found for moving keys" << endl;
            return false;  // No nodes in cluster
        }
        
        // Write to primary node
        auto primaryCache = getOrCreateNodeCache(node->id);
        if(value.hasExpiry){
            auto remainingDuration = chrono::duration_cast<chrono::seconds>(
                value.expiryTime - chrono::system_clock::now()
            );
            int ttlSeconds = remainingDuration.count();
            if (ttlSeconds > 0 && !primaryCache->setWithTtl(key, value.data, ttlSeconds)) {
                return false;
            }
        }else{
            if (!primaryCache->set(key, value.data)) {
                return false;
            }
        }
        
        // Replicate to replica nodes
        auto replicas = hashRing.getReplicaNodes(key, replicationFactor - 1);
        
        for (const auto& replicaNode : replicas) {
            auto replicaCache = getOrCreateNodeCache(replicaNode->id);
            if(value.hasExpiry){
                auto remainingDuration = chrono::duration_cast<chrono::seconds>(
                    value.expiryTime - chrono::system_clock::now()
                );
                int ttlSeconds = remainingDuration.count();
                if (ttlSeconds > 0 && !replicaCache->setWithTtl(key, value.data, ttlSeconds)) {
                    return false;
                }
            }else{
                if (!replicaCache->set(key, value.data)) {
                    return false;
                }
            }
        }
    }

    return true;
}

shared_ptr<Cache> DistributedCache::getOrCreateNodeCache(const string& nodeId) {
    auto it = nodeCache.find(nodeId);
    if (it != nodeCache.end()) {
        return it->second;
    }
    
    // Create new cache for this node
    auto cache = make_shared<Cache>(evictionPolicy, maxCapacityPerNode);
    nodeCache[nodeId] = cache;
    return cache;
}

bool DistributedCache::set(const string& key, const string& value) {
    unique_lock<shared_mutex> lock(mutex);
    
    // Get healthy nodes (skip failed ones)
    auto healthyNodes = getHealthyNodes(key, replicationFactor);
    if (healthyNodes.empty()) {
        return false;  // No healthy nodes available
    }
    
    // Write to primary healthy node
    auto primaryNode = healthyNodes[0];
    auto primaryCache = getOrCreateNodeCache(primaryNode->id);
    if (!primaryCache->set(key, value)) {
        return false;
    }
    
    // Replicate to remaining healthy replica nodes
    for (size_t i = 1; i < healthyNodes.size(); ++i) {
        auto replicaCache = getOrCreateNodeCache(healthyNodes[i]->id);
        replicaCache->set(key, value);  // Write to replica (ignore capacity errors for replicas)
    }
    
    return true;
}

bool DistributedCache::setWithTtl(const string& key, const string& value, int ttlSeconds) {
    unique_lock<shared_mutex> lock(mutex);
    
    // Get healthy nodes (skip failed ones)
    auto healthyNodes = getHealthyNodes(key, replicationFactor);
    if (healthyNodes.empty()) {
        return false;  // No healthy nodes available
    }
    
    // Write to primary healthy node with TTL
    auto primaryNode = healthyNodes[0];
    auto primaryCache = getOrCreateNodeCache(primaryNode->id);
    if (!primaryCache->setWithTtl(key, value, ttlSeconds)) {
        return false;
    }
    
    // Replicate to remaining healthy replica nodes with same TTL
    for (size_t i = 1; i < healthyNodes.size(); ++i) {
        auto replicaCache = getOrCreateNodeCache(healthyNodes[i]->id);
        replicaCache->setWithTtl(key, value, ttlSeconds);  // Write to replica
    }
    
    return true;
}

optional<string> DistributedCache::get(const string& key) {
    shared_lock<shared_mutex> lock(mutex);
    
    // Step 1: Check primary node first
    auto primaryNode = hashRing.findNode(key);
    bool primaryFailed = false;
    
    if (primaryNode) {
        if (failedNodes.find(primaryNode->id) == failedNodes.end()) {
            // Primary is healthy - check it
            auto primaryCache = getOrCreateNodeCache(primaryNode->id);
            auto value = primaryCache->get(key);
            if (value) {
                return value;  // Found on primary
            }
            // Primary is healthy but key not found - this is a cache miss, return nullopt
            return nullopt;
        } else {
            // Primary is failed - need to search other nodes
            primaryFailed = true;
        }
    } else {
        // No primary node (empty cluster)
        return nullopt;
    }
    
    // Step 2: Check replica nodes (only if primary failed)
    if (primaryFailed) {
        auto replicas = hashRing.getReplicaNodes(key, replicationFactor - 1);
        for (const auto& replicaNode : replicas) {
            // Skip failed nodes
            if (failedNodes.find(replicaNode->id) != failedNodes.end()) {
                continue;
            }
            
            auto replicaCache = getOrCreateNodeCache(replicaNode->id);
            auto value = replicaCache->get(key);
            if (value) {
                return value;  // Found on replica
            }
        }
    }
    
    return nullopt;  // Not found anywhere
}

bool DistributedCache::exists(const string& key) {
    shared_lock<shared_mutex> lock(mutex);
    
    // Step 1: Check primary node first
    auto primaryNode = hashRing.findNode(key);
    bool primaryFailed = false;
    
    if (primaryNode) {
        if (failedNodes.find(primaryNode->id) == failedNodes.end()) {
            // Primary is healthy - check it
            auto primaryCache = getOrCreateNodeCache(primaryNode->id);
            if (primaryCache->exists(key)) {
                return true;  // Found on primary
            }
            // Primary is healthy but key not found - this is a cache miss, return false
            return false;
        } else {
            // Primary is failed - need to search other nodes
            primaryFailed = true;
        }
    } else {
        // No primary node (empty cluster)
        cout << "WARNING: cluster doesn't contain any nodes" << endl;
        return false;
    }
    
    // Step 2: Check replica nodes (only if primary failed)
    if (primaryFailed) {
        auto replicas = hashRing.getReplicaNodes(key, replicationFactor - 1);
        for (const auto& replicaNode : replicas) {
            // Skip failed nodes
            if (failedNodes.find(replicaNode->id) != failedNodes.end()) {
                continue;
            }
            
            auto replicaCache = getOrCreateNodeCache(replicaNode->id);
            if (replicaCache->exists(key)) {
                return true;  // Found on replica
            }
        }
    }
    
    return false;  // Not found anywhere
}

// todo : review this 
bool DistributedCache::del(const string& key) {
    unique_lock<shared_mutex> lock(mutex);
    
    // Get healthy nodes (skip failed ones)
    auto healthyNodes = getHealthyNodes(key, replicationFactor);
    if (healthyNodes.empty()) {
        return false;  // No healthy nodes available
    }
    
    bool deleted = false;
    
    // Delete from all healthy nodes to keep consistency
    for (const auto& node : healthyNodes) {
        auto cache = getOrCreateNodeCache(node->id);
        if (cache->del(key)) {
            deleted = true;  // At least one deletion succeeded
        }
    }
    
    return deleted;
}

size_t DistributedCache::size() {
    shared_lock<shared_mutex> lock(mutex);
    
    size_t total = 0;
    for (const auto& [nodeId, cache] : nodeCache) {
        total += cache->size();
    }
    return total;
}

void DistributedCache::clear() {
    unique_lock<shared_mutex> lock(mutex);
    
    for (auto& [nodeId, cache] : nodeCache) {
        cache->clear();
    }
}

string DistributedCache::findKeyNode(const string& key) const {
    shared_lock<shared_mutex> lock(mutex);
    
    auto node = hashRing.findNode(key);
    return node ? node->id : "";
}

vector<string> DistributedCache::getKeyReplicas(const string& key, size_t n) const {
    shared_lock<shared_mutex> lock(mutex);
    
    auto replicas = hashRing.getReplicaNodes(key, n);
    vector<string> result;
    for (const auto& node : replicas) {
        result.push_back(node->id);
    }
    return result;
}

vector<string> DistributedCache::getAllNodes() const {
    shared_lock<shared_mutex> lock(mutex);
    return hashRing.getAllNodes();
}

size_t DistributedCache::getNodeSize(const string& nodeId) const {
    shared_lock<shared_mutex> lock(mutex);
    
    auto it = nodeCache.find(nodeId);
    if (it != nodeCache.end()) {
        return it->second->size();
    }
    return 0;
}

DistributedCache::Stats DistributedCache::getStats() const {
    shared_lock<shared_mutex> lock(mutex);
    
    auto hashStats = hashRing.getStats();
    return {
        hashStats.totalNodes,
        hashStats.ringSize,
        maxCapacityPerNode,
        hashStats.totalVirtualNodes / (hashStats.totalNodes > 0 ? hashStats.totalNodes : 1),
        replicationFactor
    };
}

bool DistributedCache::markNodeFailed(const string& nodeId) {
    unique_lock<shared_mutex> lock(mutex);
    
    // Check if node exists
    if (!hashRing.hasNode(nodeId)) {
        return false;
    }
    
    // Check if already failed
    if (failedNodes.find(nodeId) != failedNodes.end()) {
        return false;
    }
    
    // Mark as failed
    failedNodes.insert(nodeId);
    return true;
}

bool DistributedCache::isNodeFailed(const string& nodeId) const {
    shared_lock<shared_mutex> lock(mutex);
    return failedNodes.find(nodeId) != failedNodes.end();
}

vector<string> DistributedCache::getFailedNodes() const {
    shared_lock<shared_mutex> lock(mutex);
    vector<string> result;
    for (const auto& nodeId : failedNodes) {
        result.push_back(nodeId);
    }
    return result;
}

vector<shared_ptr<Node>> DistributedCache::getHealthyNodes(const string& key, size_t maxNodes) const {
    vector<shared_ptr<Node>> healthyNodes;
    
    // Get primary node
    auto primaryNode = hashRing.findNode(key);
    if (!primaryNode) {
        return healthyNodes;  // No nodes in cluster
    }
    
    // Strategy: Get replica nodes in deterministic ring order
    // This ensures we only check nodes that SHOULD have the data based on consistent hashing
    // We request enough replicas to cover the maxNodes we want to check
    
    // Get replica nodes in ring order (deterministic)
    // Request enough replicas to fill maxNodes (accounting for primary)
    // Request a bit more to account for failed nodes in the list
    size_t replicasToRequest = maxNodes * 2;  // Request 2x to account for failed nodes
    if (replicasToRequest > hashRing.nodeCount()) {
        replicasToRequest = hashRing.nodeCount();
    }
    auto allReplicas = hashRing.getReplicaNodes(key, replicasToRequest);
    
    // Add primary if healthy (should be first)
    if (failedNodes.find(primaryNode->id) == failedNodes.end()) {
        healthyNodes.push_back(primaryNode);
    }
    
    // Add healthy replicas in ring order (deterministic)
    for (const auto& replica : allReplicas) {
        if (healthyNodes.size() >= maxNodes) {
            break;
        }
        
        // Skip if already added (primary might be in replicas list)
        bool alreadyAdded = false;
        for (const auto& existing : healthyNodes) {
            if (existing->id == replica->id) {
                alreadyAdded = true;
                break;
            }
        }
        
        // Add if not already added and not failed
        if (!alreadyAdded && failedNodes.find(replica->id) == failedNodes.end()) {
            healthyNodes.push_back(replica);
        }
    }
    
    return healthyNodes;
}
