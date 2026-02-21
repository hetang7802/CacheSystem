#ifndef DISTRIBUTED_CACHE_H
#define DISTRIBUTED_CACHE_H

#include "cache.h"
#include "consistent_hash.h"
#include <map>
#include <memory>
#include <shared_mutex>
#include <unordered_set>
#include <mutex>
#include "anti_entropy.h"

using namespace std;

/**
 * Distributed Cache System with Data Replication
 * 
 * Simulates a distributed cache using consistent hashing to partition
 * keys across multiple cache nodes. Implements data replication for
 * fault tolerance - each key is stored on primary node + replica nodes.
 * 
 * In a real system, these would be separate processes/machines 
 * communicating over network. For this simulation, each node is a 
 * local Cache instance.
 */
class DistributedCache {
public:
    /**
     * Constructor
     * @param numVirtualNodes Virtual nodes per physical node (affects distribution)
     * @param maxCapacityPerNode Max keys per node (0 = unlimited)
     * @param evictionPolicy Eviction policy for each node
     * @param replicationFactor Number of copies of each key (default 2: primary + 1 replica)
     */
    DistributedCache(int numVirtualNodes = 150,
                     size_t maxCapacityPerNode = 0,
                     Cache::EvictionType evictionPolicy = Cache::EvictionType::NONE,
                     size_t replicationFactor = 2);

    /**
     * Add a node to the distributed system
     * @param nodeId Unique node identifier
     * @return true if added successfully
     */
    bool addNode(const string& nodeId);

    /**
     * Remove a node from the distributed system
     * @param nodeId Node identifier to remove
     * @return true if removed successfully
     */
    bool removeNode(const string& nodeId);

    /**
     * Set a key-value pair with data replication
     * Writes to primary node + all replica nodes determined by consistent hashing
     * @param key The key
     * @param value The value
     * @return true if successful
     */
    bool set(const string& key, const string& value);

    /**
     * Set with TTL and data replication
     * Writes to primary node + all replica nodes with same TTL
     * @param key The key
     * @param value The value
     * @param ttlSeconds Time to live
     * @return true if successful
     */
    bool setWithTtl(const string& key, const string& value, int ttlSeconds);

    /**
     * Get a value (reads from primary, falls back to replicas if needed)
     * @param key The key
     * @return Value if found, nullopt otherwise
     */
    optional<string> get(const string& key);

    /**
     * Check if key exists
     */
    bool exists(const string& key);

    /**
     * Delete a key from primary and all replicas
     * Ensures deletion is consistent across all replica nodes
     */
    void del(const string& key);

    /**
     * Get total number of keys across all nodes
     */
    size_t size();

    /**
     * Clear all nodes
     */
    void clear();

    /**
     * Find which node a key maps to
     * @param key The key
     * @return Node ID, or empty string if not found
     */
    string findKeyNode(const string& key) const;

    /**
     * Get replicas for a key (for fault tolerance planning)
     * @param key The key
     * @param n Number of replicas
     * @return List of node IDs
     */
    vector<string> getKeyReplicas(const string& key, size_t n = 2) const;

    /**
     * Get all nodes in the cluster
     * @return Vector of node IDs
     */
    vector<string> getAllNodes() const;

    /**
     * Get all healthy nodes in the cluster
     * @return Vector of node IDs
     */
    vector<string> getAllHealthyNodes() const;

    /**
     * Get number of nodes
     */
    size_t nodeCount() const { return hashRing.nodeCount(); }

    /**
     * Get statistics for a specific node
     * @param nodeId Node identifier
     * @return Cache size for that node
     */
    size_t getNodeSize(const string& nodeId) const;

    /**
     * Get overall statistics
     */
    struct Stats {
        size_t totalNodes;
        size_t totalKeys;
        size_t maxCapacityPerNode;
        size_t virtualNodesPerPhysicalNode;
        size_t replicationFactor;  // Number of copies per key
    };
    
    Stats getStats() const;

    /**
     * Mark a node as failed (simulates node failure)
     * The node will be excluded from operations
     * @param nodeId Node identifier to mark as failed
     * @return true if node was marked as failed, false if not found or already failed
     */
    bool markNodeFailed(const string& nodeId);

    /**
     * Check if a node is currently marked as failed
     * @param nodeId Node identifier
     * @return true if node is failed, false otherwise
     */
    bool isNodeFailed(const string& nodeId) const;

    /**
     * Get list of all failed nodes
     * @return Vector of failed node IDs
     */
    vector<string> getFailedNodes() const;

    // Check replication health for a key
    size_t getActualReplicationCount(const string& key) const;

    // Re-replicate a key to restore replication factor
    bool repairKey(const string& key);

    // Get all keys stored on a specific node
    vector<string> getKeysOnNode(const string& nodeId) const;

    // getter for replication factor
    size_t getReplicationFactor() const {return replicationFactor;};  // Number of copies per key (1 primary + replicas)

private:
    unique_ptr<AntiEntropyService> antiEntropyService;

    // Hash ring for key distribution
    ConsistentHashRing hashRing;
    
    // Cache instances for each node
    map<string, shared_ptr<Cache>> nodeCache;
    
    // Configuration
    size_t maxCapacityPerNode;
    Cache::EvictionType evictionPolicy;
    size_t replicationFactor;  // Number of copies per key (1 primary + replicas)
    
    // Thread safety
    mutable shared_mutex mutex;

    // Failed nodes tracking
    unordered_set<string> failedNodes;
    /**
     * create cache for a node
     */
    shared_ptr<Cache> createNodeCache(const string& nodeId);

    /**
     * Get cache for a node
     */
    shared_ptr<Cache> getNodeCache(const string& nodeId);

    // Get nodes that should have a key (based on consistent hashing)
    vector<string> getExpectedNodesForKey(const string &key, unique_lock<shared_mutex> &heldLock) const;

    bool writeWithOptionalTtl(shared_ptr<Cache> cache,
                              const string &key,
                              const Value &value);
};

#endif // DISTRIBUTED_CACHE_H
