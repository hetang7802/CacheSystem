#ifndef EVICTION_H
#define EVICTION_H

#include <string>
#include <list>
#include <unordered_map>
#include <memory>

using namespace std;

/**
 * Base class for eviction policies
 * Determines which key to remove when cache reaches max capacity
 */
class EvictionPolicy {
public:
    virtual ~EvictionPolicy() = default;

    /**
     * Record that a key was accessed (for LRU/LFU tracking)
     */
    virtual void onAccess(const string& key) = 0;

    /**
     * Record that a key was added
     */
    virtual void onAdd(const string& key) = 0;

    /**
     * Record that a key was removed
     */
    virtual void onRemove(const string& key) = 0;

    /**
     * Get the key to evict (remove from cache)
     * @return Key to evict, or empty string if nothing to evict
     */
    virtual string evict() = 0;

    /**
     * Get current number of tracked keys
     */
    virtual size_t size() const = 0;

    /**
     * Clear all tracking
     */
    virtual void clear() = 0;

    /**
     * Get policy name for display
     */
    virtual string policyName() const = 0;
};

/**
 * LRU (Least Recently Used) Eviction Policy
 * 
 * Evicts the key that was accessed longest time ago
 * Uses a doubly-linked list + hash map for O(1) operations
 * 
 * Implementation:
 * - list<string>: ordered keys (front = most recent, back = least recent)
 * - unordered_map<string, iterator>: O(1) lookup to update position
 * - On access: move key to front
 * - On evict: remove from back
 */
class LRUEvictionPolicy : public EvictionPolicy {
public:
    void onAccess(const string& key) override;
    void onAdd(const string& key) override;
    void onRemove(const string& key) override;
    string evict() override;
    size_t size() const override;
    void clear() override;
    string policyName() const override { return "LRU"; }

private:
    // List: front = most recently used, back = least recently used
    list<string> accessOrder;
    // Map: key -> iterator in list (for O(1) removal/update)
    unordered_map<string, list<string>::iterator> keyToIterator;
};

/**
 * LFU (Least Frequently Used) Eviction Policy
 * 
 * Evicts the key that has been accessed least frequently
 * In case of tie, evicts the least recently used among tied keys
 * 
 * Implementation:
 * - unordered_map<string, frequency>: track access count
 * - unordered_map<frequency, list<string>>: organize by frequency
 * - min_frequency: track smallest frequency for quick eviction
 * - On access: increment frequency, move to new frequency bucket
 * - On evict: remove first key from min_frequency bucket
 */
class LFUEvictionPolicy : public EvictionPolicy {
public:
    void onAccess(const string& key) override;
    void onAdd(const string& key) override;
    void onRemove(const string& key) override;
    string evict() override;
    size_t size() const override;
    void clear() override;
    string policyName() const override { return "LFU"; }

private:
    // Track frequency of each key
    unordered_map<string, int> keyFrequency;
    
    // Organize keys by frequency: freq -> [key1, key2, ...]
    // Keys in list are ordered by insertion (oldest first for tie-breaking)
    unordered_map<int, list<string>> frequencyBuckets;
    
    // Minimum frequency (for quick eviction)
    int minFrequency = 0;
};

#endif // EVICTION_H
