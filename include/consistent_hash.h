#ifndef CONSISTENT_HASH_H
#define CONSISTENT_HASH_H

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>


using namespace std;

/**
 * Node in the consistent hash ring
 * Represents a physical server/cache instance
 */
struct Node {
    string id;        // Unique identifier (e.g., "node-1", "192.168.1.1:6379")
    string address;   // Network address for future distributed ops
    
    Node(const string& nodeId, const string& addr = "")
        : id(nodeId), address(addr.empty() ? nodeId : addr) {}
    
    bool operator==(const Node& other) const {
        return id == other.id;
    }
};

/**
 * Consistent Hashing Ring
 * 
 * Implements a consistent hash ring for distributing keys across multiple nodes.
 * Uses virtual nodes (replicas) to improve load balancing and minimize key movement
 * during node additions/removals.
 * 
 * Properties:
 * - O(log n) lookup time for key -> node mapping (binary search on ring)
 * - Minimal key redistribution when nodes are added/removed
 * - Virtual nodes reduce hotspots and improve distribution
 * - Ring is circular: hash values wrap around
 */
class ConsistentHashRing {
public:
    /**
     * Constructor
     * @param numVirtualNodes Number of virtual copies per physical node (default: 150)
     *                        Higher = better distribution, more memory/cpu
     */
    ConsistentHashRing(int numVirtualNodes = 150);

    /**
     * Add a node to the ring
     * @param node The node to add
     * @return true if added, false if already exists
     */
    bool addNode(const Node& node);

    /**
     * Remove a node from the ring
     * @param nodeId The node ID to remove
     * @return true if removed, false if not found
     */
    bool removeNode(const string& nodeId);

    /**
     * Find which node a key belongs to
     * @param key The key to look up
     * @return The node that owns this key, or nullptr if ring is empty
     */
    shared_ptr<Node> findNode(const string& key) const;

    /**
     * Get all nodes in the ring
     * @return Vector of node IDs
     */
    vector<string> getAllNodes() const;

    /**
     * Get number of physical nodes
     */
    size_t nodeCount() const { return nodes.size(); }

    /**
     * Get number of total points on the ring (nodes * virtualNodes)
     */
    size_t ringSize() const { return ring.size(); }

    /**
     * Get the next N nodes for a key (for replication)
     * @param key The key to look up
     * @param n Number of nodes to return
     * @return Vector of nodes
     */
    vector<shared_ptr<Node>> getReplicaNodes(const string& key, size_t n = 2) const;

    /**
     * Check if a node exists
     * @param nodeId The node ID
     */
    bool hasNode(const string& nodeId) const;

    /**
     * Clear all nodes from the ring
     */
    void clear();

    /**
     * Get statistics about the ring
     */
    struct Stats {
        size_t totalNodes;
        size_t totalVirtualNodes;
        size_t ringSize;
    };
    
    Stats getStats() const {
        return { nodes.size(), nodes.size() * virtualNodesPerPhysicalNode, ring.size() };
    }

private:
    // Ring: hash value -> node
    // Using map for O(log n) lookup
    map<uint64_t, shared_ptr<Node>> ring;
    
    // Nodes: node ID -> node
    map<string, shared_ptr<Node>> nodes;
    
    // Number of virtual nodes per physical node
    int virtualNodesPerPhysicalNode;

    /**
     * Hash function using MD5-style hashing
     * Produces a 64-bit hash value
     * @param input String to hash
     * @return Hash value
     */
    uint64_t customHash(const string& input) const;

    /**
     * Generate virtual node identifier
     * @param nodeId Physical node ID
     * @param replicaIndex Virtual node index (0 to virtualNodesPerPhysicalNode-1)
     * @return Virtual node identifier string
     */
    string getVirtualNodeId(const string& nodeId, int replicaIndex) const;

    /**
     * Add virtual nodes to the ring for a physical node
     * @param node The physical node
     */
    void addVirtualNodes(const shared_ptr<Node>& node);

    /**
     * Remove virtual nodes from the ring for a physical node
     * @param nodeId The physical node ID
     */
    void removeVirtualNodes(const string& nodeId);
};

#endif // CONSISTENT_HASH_H
