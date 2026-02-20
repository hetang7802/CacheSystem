#include "distributed_cache.h"
#include <mutex>
#include <iostream>
#include <assert.h>

using namespace std;

DistributedCache::DistributedCache(int numVirtualNodes,
                                   size_t maxCapacityPerNode,
                                   Cache::EvictionType evictionPolicy,
                                   size_t replicationFactor)
    : hashRing(numVirtualNodes),
      maxCapacityPerNode(maxCapacityPerNode),
      evictionPolicy(evictionPolicy),
      replicationFactor(replicationFactor) {
    this->antiEntropyService = make_unique<AntiEntropyService>(this, chrono::seconds(30));
    antiEntropyService->start(); // Starts automatically
}

bool DistributedCache::addNode(const string& nodeId) {
    unique_lock<shared_mutex> lock(mutex);
    
    Node node(nodeId);
    createNodeCache(nodeId);
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
    
    bool allSuccess = true;

    for(auto &itr : dataToRedistribute){
        string key = itr.first;
        Value value = itr.second;
        
        // Skip expired keys
        if (value.isExpired()) {
            continue;
        }
        
        auto node = hashRing.findPrimaryNode(key, failedNodes);
        if (!node) {
            return false;  // No nodes in cluster
        }
        
        // Write to primary node
        auto primaryCache = getNodeCache(node->id);
        if (!writeWithOptionalTtl(primaryCache, key, value)) {
            // cout << "[ERROR] : write to node " << node->id << " failed" << endl;
            allSuccess = false;
        }
        
        // Replicate to replica nodes
        auto replicas = hashRing.getReplicaNodes(key, replicationFactor - 1, failedNodes);
        
        for (const auto& replicaNode : replicas) {
            auto replicaCache = getNodeCache(replicaNode->id);
            if (!writeWithOptionalTtl(replicaCache, key, value)) {
                // cout << "[ERROR] : write to node " << replicaNode->id << " failed" << endl;
                allSuccess = false;
            }
        }
    }
    return allSuccess;
}

bool DistributedCache::writeWithOptionalTtl(shared_ptr<Cache> cache, 
                                            const string& key, 
                                            const Value& value) {
    if (value.hasExpiry) {
        auto remainingDuration = chrono::duration_cast<chrono::seconds>(
            value.expiryTime - chrono::system_clock::now()
        );
        int ttlSeconds = remainingDuration.count();
        if (ttlSeconds > 0) {
            return cache->setWithTtl(key, value.data, ttlSeconds);
        }
        // TTL expired, don't write
        return false;
    } else {
        return cache->set(key, value.data);
    }
}

shared_ptr<Cache> DistributedCache::createNodeCache(const string& nodeId){
    // Create new cache for this node
    auto cache = make_shared<Cache>(evictionPolicy, maxCapacityPerNode);
    nodeCache[nodeId] = cache;
    return cache;
}

shared_ptr<Cache> DistributedCache::getNodeCache(const string& nodeId) {
    auto it = nodeCache.find(nodeId);
    if (it != nodeCache.end()) {
        return it->second;
    }

    throw invalid_argument("could not find node with provided id " + nodeId);
}

bool DistributedCache::set(const string& key, const string& value) {
    unique_lock<shared_mutex> lock(mutex);
    
    // Get first healthy node (primary or next healthy node if primary failed)
    auto primaryNode = hashRing.findPrimaryNode(key, failedNodes);
    if (!primaryNode) {
        cout << "[ERROR] : No healthy nodes available" << endl;
        return false;
    }
    
    auto primaryCache = getNodeCache(primaryNode->id);
    if (!primaryCache->set(key, value)) {
        return false;
    }
    
    // Get healthy replicas (automatically filters failed nodes)
    auto replicas = hashRing.getReplicaNodes(key, replicationFactor - 1, failedNodes);
    
    for (const auto& replicaNode : replicas) {
        auto replicaCache = getNodeCache(replicaNode->id);
        replicaCache->set(key, value);
    }
    
    return true;
}

bool DistributedCache::setWithTtl(const string& key, const string& value, int ttlSeconds) {
    unique_lock<shared_mutex> lock(mutex);
    
    // Get first healthy node (primary or next healthy node if primary failed)
    auto primaryNode = hashRing.findPrimaryNode(key, failedNodes);
    if (!primaryNode) {
        cout << "[ERROR] : No healthy nodes available" << endl;
        return false;
    }
    
    auto primaryCache = getNodeCache(primaryNode->id);
    if (!primaryCache->setWithTtl(key, value, ttlSeconds)) {
        return false;
    }
    
    // Get healthy replicas (automatically filters failed nodes)
    auto replicas = hashRing.getReplicaNodes(key, replicationFactor - 1, failedNodes);
    
    for (const auto& replicaNode : replicas) {
        auto replicaCache = getNodeCache(replicaNode->id);
        replicaCache->setWithTtl(key, value, ttlSeconds);
    }
    
    return true;
}

optional<string> DistributedCache::get(const string& key) {
    shared_lock<shared_mutex> lock(mutex);
    
    // Automatically gets first healthy node (primary or first healthy replica)
    auto healthyNode = hashRing.findPrimaryNode(key, failedNodes);
    if (!healthyNode) {
        return nullopt;  // All nodes for this key are failed
    }
    
    auto cache = getNodeCache(healthyNode->id);
    return cache->get(key);
}

bool DistributedCache::exists(const string& key) {
    shared_lock<shared_mutex> lock(mutex);
        
    // Get first healthy node that should have this key
    auto healthyNode = hashRing.findPrimaryNode(key, failedNodes);
    if (!healthyNode) {
        return false;
    }
    
    auto cache = getNodeCache(healthyNode->id);
    return cache->exists(key);
}

// todo : review this 
void DistributedCache::del(const string& key) {
    unique_lock<shared_mutex> lock(mutex);
    
    // Get all healthy nodes that should have this key
    auto primaryNode = hashRing.findPrimaryNode(key, failedNodes);
    if (!primaryNode) {
        return ;  // No healthy nodes
    }
    
    // Delete from primary
    auto primaryCache = getNodeCache(primaryNode->id);
    if (!primaryCache->del(key)) {
        cout << "[WARNING] : problem occured while deleting " << key << " from " << primaryNode->id << endl;
    }
    
    // Delete from replicas
    auto replicas = hashRing.getReplicaNodes(key, replicationFactor - 1, failedNodes);
    for (const auto& replica : replicas) {
        auto cache = getNodeCache(replica->id);
        if (!cache->del(key)) {
            cout << "[WARNING] : problem occured while deleting " << key << " from " << primaryNode->id << endl;
        }
    }
    
    return ;
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
    
    auto node = hashRing.findPrimaryNode(key, failedNodes);
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

vector<string> DistributedCache::getKeysOnNode(const string& nodeId) const {
    shared_lock<shared_mutex> lock(mutex);
    
    vector<string> keys;
    if(failedNodes.find(nodeId)!=failedNodes.end())return keys;
    
    auto it = nodeCache.find(nodeId);
    if (it == nodeCache.end()) {
        return keys;
    }
    
    // Get all data from this node's cache
    auto nodeData = it->second->getAllData();
    
    for (const auto& [key, value] : nodeData) {
        // Skip expired keys
        if (!value.isExpired()) {
            keys.push_back(key);
        }
    }
    
    return keys;
}

size_t DistributedCache::getActualReplicationCount(const string& key) const {
    shared_lock<shared_mutex> lock(mutex);
    
    size_t count = 0;
    
    // Check all nodes to see which ones have this key
    for (const auto& [nodeId, cache] : nodeCache) {
        // Skip failed nodes
        if (failedNodes.find(nodeId) != failedNodes.end()) {
            continue;
        }
        
        if (cache->exists(key)) {
            count++;
        }
    }
    
    return count;
}

vector<string> DistributedCache::getAllHealthyNodes() const {
    shared_lock<shared_mutex> lock(mutex);
    vector<string> allNodes = getAllNodes();
    vector<string> healthyNodes ;
    for(string node: allNodes){
        if(failedNodes.find(node)==failedNodes.end()){
            healthyNodes.push_back(node);
        }
    }
    return hashRing.getAllNodes();
}

bool DistributedCache::repairKey(const string& key) {
    unique_lock<shared_mutex> lock(mutex);
    
    // Find a healthy node that has this key
    auto expectedNodes = getExpectedNodesForKey(key, lock);

    
    // Find a healthy source node with the data
    optional<Cache::ValueWithTtl> valueWithTtl;
    string sourceNodeId = "";
    
    for (const auto& nodeId : expectedNodes) {
        if (failedNodes.find(nodeId) != failedNodes.end()) {
            continue;  // Skip failed nodes
        }
        auto cacheIt = nodeCache.find(nodeId);
        if (cacheIt != nodeCache.end()) {
            valueWithTtl = cacheIt->second->getValueWithTtl(key);
            if (valueWithTtl) {
                sourceNodeId = nodeId;
                break;  // Found it!
            }
        }
    }
    // cout << "[DEBUG] : source node " << sourceNodeId << endl;
    
    if (!valueWithTtl) {
        // No healthy node has this key - data is lost
        // cout << "[DEBUG] : no healthy node found iwth the key" << key << endl;
        return false;
    }
    
    // Replicate to all expected nodes that don't have it
    bool success = true;
    
    for (const auto& nodeId : expectedNodes) {
        // cout << "[DEBUG] : trying to replicate in " << nodeId << endl;

        if(nodeId == sourceNodeId || failedNodes.find(nodeId)!=failedNodes.end()) {
            // cout << "[DEBUG] : node either source or failed " << endl;
            continue;
        }
        // cout << "[DEBUG] : trying to replicate in " << nodeId << endl;
        
        auto targetCache = getNodeCache(nodeId);
        
        // Check if already has the key
        if (targetCache->exists(key)) {
            // cout << "[DEBUG] : key already exists in " << nodeId << endl;
            continue;
        }
        
        // Replicate with TTL if applicable
        // todo : replace this with writeWithOptionalTtl
        if (valueWithTtl->hasExpiry && valueWithTtl->remainingTtl && *valueWithTtl->remainingTtl > 0) {
            // cout << "[DEBUG] : setting with ttl to " << nodeId << endl;
            if(!targetCache->setWithTtl(key, valueWithTtl->data, *valueWithTtl->remainingTtl)){
                success = false;
            }
        } else {
            // cout << "[DEBUG] : setting without ttl to " << nodeId << endl;
            if(!targetCache->set(key, valueWithTtl->data)){
                success = false;
            }
        }
    }
    if(success){
        // cout << "[DEBUG] : repair successfull for key " << key << endl; 
    }else{
        // cout << "[DEBUG] : could not duplicate key to other nodes " << key << endl;
    } 
    return success;
}


vector<string> DistributedCache::getExpectedNodesForKey(const string& key, 
        unique_lock<shared_mutex>& heldLock) const {
        
    assert(heldLock.owns_lock());
    
    vector<string> expectedNodes;
    
    // Get primary node
    auto primaryNode = hashRing.findPrimaryNode(key, failedNodes);
    if (!primaryNode) {
        return expectedNodes;
    }
    
    expectedNodes.push_back(primaryNode->id);
    
    // Get replica nodes
    auto replicas = hashRing.getReplicaNodes(key, replicationFactor - 1, failedNodes);
    for (const auto& replica : replicas) {
        expectedNodes.push_back(replica->id);
    }
    
    return expectedNodes;
}