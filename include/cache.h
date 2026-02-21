#ifndef CACHE_H
#define CACHE_H

#include <unordered_map>
#include <string>
#include <chrono>
#include <shared_mutex>
#include <optional>
#include "eviction.h"
#include <atomic>
#include <vector>

using namespace std;

/**
 * Value wrapper that stores the actual data and metadata
 */
struct Value {
    string data;
    chrono::system_clock::time_point expiryTime;
    bool hasExpiry;

    Value() : data(""), hasExpiry(false) {}
    
    Value(const string& d) 
        : data(d), hasExpiry(false) {}
    
    Value(const string& d, chrono::system_clock::time_point exp)
        : data(d), expiryTime(exp), hasExpiry(true) {}

    /**
     * Check if this value has expired
     */
    bool isExpired() const {
        if (!hasExpiry) return false;
        return chrono::system_clock::now() > expiryTime;
    }
};

/**
 * In-memory cache with TTL support, eviction policies, and thread safety
 * Phase 1: Core cache engine
 * Phase 2: Eviction policies (LRU/LFU)
 * Phase 3: Lock sharding for better concurrency
 */
class Cache {
public:
    struct ValueWithTtl {
        string data;
        bool hasExpiry;
        optional<int> remainingTtl;  // seconds remaining, nullopt if no expiry
    };
    // Eviction policy types
    enum class EvictionType {
        NONE,  // No eviction - error when full
        LRU,   // Least Recently Used
        LFU    // Least Frequently Used
    };

    // Number of shards for lock sharding (reduces contention)
    static constexpr size_t NUM_SHARDS = 16;

    /**
     * Default constructor (no eviction, unlimited size)
     */
    Cache();

    /**
     * Constructor with eviction policy
     * @param policy_type Type of eviction (LRU, LFU, or NONE)
     * @param max_capacity Maximum number of keys before eviction
     */
    Cache(EvictionType policy_type, size_t max_capacity);

    ~Cache();

    /**
     * Set a key-value pair (no expiry)
     * @param key The key
     * @param value The value
     * @return true if set successfully, false if capacity exceeded
     */
    bool set(const string& key, const string& value);

    /**
     * Set a key-value pair with TTL
     * @param key The key
     * @param value The value
     * @param ttlSeconds Time to live in seconds
     * @return true if set successfully, false if capacity exceeded
     */
    bool setWithTtl(const string& key, const string& value, 
                    int ttlSeconds);

    /**
     * Get a value by key
     * @param key The key
     * @return The value if found and not expired, nullopt otherwise
     */
    optional<string> get(const string& key);

    /**
     * Check if a key exists (and is not expired)
     * @param key The key
     * @return true if key exists and is not expired
     */
    bool exists(const string& key);

    /**
     * Delete a key
     * @param key The key
     * @return true if key was deleted, false if it didn't exist
     */
    bool del(const string& key);

    /**
     * Get the number of keys in the cache (excluding expired ones)
     * @return Number of valid keys
     */
    size_t size();

    /**
     * Clear all keys from the cache
     */
    void clear();

    /**
     * Get eviction policy name
     * @return "LRU", "LFU", or "NONE"
     */
    string evictionPolicy() const;

    /**
     * Get cache capacity
     * @return Max capacity, or 0 if unlimited
     */
    size_t capacity() const { return maxCapacity; }

    /**
     * Get eviction stats
     * @return Number of keys that have been evicted
     */
    // size_t evictionCount() const { return evictedCount;}

    /**
     * Clean up expired keys (internal housekeeping)
     * @return Number of keys removed
     */
    size_t cleanupExpired();

    /**
     * Get all key value pairs from the cache
     * @return Vector of all key names
     */
    unordered_map<string, Value> getAllData() const;

    optional<ValueWithTtl> getValueWithTtl(const string& key);

private:
    /**
     * Shard structure - each shard has its own lock and data to reduce contention
     */
    struct Shard {
        mutable shared_mutex mutex;
        unordered_map<string, Value> store;
        unique_ptr<EvictionPolicy> evictionPolicy;
        size_t evictedCount;
        size_t shardCapacity;
    };
    
    vector<unique_ptr<Shard>> shards;
    size_t maxCapacity;
    EvictionType evictionPolicyType;

    /**
     * Get shard index for a key (hash-based distribution)
     */
    size_t getShard(const string& key) const {
        hash<string> hasher;
        return hasher(key) % NUM_SHARDS;
    }

    /**
     * Create eviction policy instance
     */
    unique_ptr<EvictionPolicy> createEvictionPolicy(EvictionType type);

    /**
     * Enforce capacity for a specific shard (must be called with shard lock held)
     */
    void enforceCapacityShard(size_t shardIdx);
    
    void cleanupExpiredBatch(size_t maxToClean);
};

#endif // CACHE_H
