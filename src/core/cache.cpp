#include "cache.h"
#include <algorithm>
#include <mutex>
#include <memory>
#include <iostream>

using namespace std;

// ============== CONSTRUCTOR & DESTRUCTOR ==============

Cache::Cache() 
    : maxCapacity(0), evictionPolicyType(EvictionType::NONE) {
    // Initialize all shards with unique_ptr
    for (size_t i = 0; i < NUM_SHARDS; i++) {
        auto shard = make_unique<Shard>();
        shard->evictedCount = 0;
        shard->shardCapacity = 0;
        shard->evictionPolicy = nullptr;
        shards.push_back(move(shard));
    }
}

Cache::Cache(EvictionType policyType, size_t maxCapacityVal)
    : maxCapacity(maxCapacityVal), evictionPolicyType(policyType) {
    
    // Initialize all shards with their share of capacity
    size_t capacityPerShard = (maxCapacityVal == 0) ? 0 : (maxCapacityVal / NUM_SHARDS);
    
    for (size_t i = 0; i < NUM_SHARDS; i++) {
        auto shard = make_unique<Shard>();
        shard->evictedCount = 0;
        shard->shardCapacity = capacityPerShard;
        shard->evictionPolicy = createEvictionPolicy(policyType);
        shards.push_back(move(shard));
    }
}

Cache::~Cache() {}

unique_ptr<EvictionPolicy> Cache::createEvictionPolicy(EvictionType type) {
    switch (type) {
        case EvictionType::LRU:
            return make_unique<LRUEvictionPolicy>();
        case EvictionType::LFU:
            return make_unique<LFUEvictionPolicy>();
        default:
            return nullptr;
    }
}

// ============== CORE OPERATIONS ==============

bool Cache::set(const string& key, const string& value) {
    size_t shardIdx = getShard(key);
    Shard& shard = *shards[shardIdx];
    
    unique_lock<shared_mutex> lock(shard.mutex);
    
    // Check if key exists
    bool keyExists = shard.store.find(key) != shard.store.end();
    
    // Check capacity before new insertion
    if (!keyExists && shard.shardCapacity > 0 && shard.store.size() >= shard.shardCapacity) {
        // Need to enforce capacity
        if (shard.evictionPolicy) {
            while (shard.store.size() >= shard.shardCapacity) {
                try {
                    string keyToEvict = shard.evictionPolicy->evict();
                    if (keyToEvict.empty()) break;
                    
                    auto it = shard.store.find(keyToEvict);
                    if (it != shard.store.end()) {
                        shard.store.erase(it);
                        shard.evictedCount++;
                    } else {
                        break;
                    }
                } catch (...) {
                    break;
                }
            }
        }
        
        // If still full after eviction, fail
        if (shard.store.size() >= shard.shardCapacity) {
            return false;
        }
    }
    
    // Track in eviction policy
    if (shard.evictionPolicy) {
        if (keyExists) {
            try {
                shard.evictionPolicy->onAccess(key);
            } catch (...) {}
        } else {
            try {
                shard.evictionPolicy->onAdd(key);
            } catch (...) {}
        }
    }
    
    shard.store[key] = Value(value);
    return true;
}

bool Cache::setWithTtl(const string& key, const string& value, int ttlSeconds) {
    size_t shardIdx = getShard(key);
    Shard& shard = *shards[shardIdx];
    
    unique_lock<shared_mutex> lock(shard.mutex);
    
    bool keyExists = shard.store.find(key) != shard.store.end();
    
    if (!keyExists && shard.shardCapacity > 0 && shard.store.size() >= shard.shardCapacity) {
        if (shard.evictionPolicy) {
            while (shard.store.size() >= shard.shardCapacity) {
                try {
                    string keyToEvict = shard.evictionPolicy->evict();
                    if (keyToEvict.empty()) break;
                    
                    auto it = shard.store.find(keyToEvict);
                    if (it != shard.store.end()) {
                        shard.store.erase(it);
                        shard.evictedCount++;
                    } else {
                        break;
                    }
                } catch (...) {
                    break;
                }
            }
        }
        
        if (shard.store.size() >= shard.shardCapacity) {
            return false;
        }
    }
    
    if (shard.evictionPolicy) {
        if (keyExists) {
            try {
                shard.evictionPolicy->onAccess(key);
            } catch (...) {}
        } else {
            try {
                shard.evictionPolicy->onAdd(key);
            } catch (...) {}
        }
    }
    
    auto expiryTime = chrono::system_clock::now() + chrono::seconds(ttlSeconds);
    shard.store[key] = Value(value, expiryTime);
    return true;
}

optional<string> Cache::get(const string& key) {
    size_t shardIdx = getShard(key);
    Shard& shard = *shards[shardIdx];
    
    unique_lock<shared_mutex> lock(shard.mutex);
    
    auto it = shard.store.find(key);
    if (it == shard.store.end()) {
        return nullopt;
    }

    if (it->second.isExpired()) {
        if (shard.evictionPolicy) {
            try {
                shard.evictionPolicy->onRemove(key);
            } catch (...) {}
        }
        shard.store.erase(it);
        return nullopt;
    }

    // Track access for eviction policies
    if (shard.evictionPolicy) {
        try {
            shard.evictionPolicy->onAccess(key);
        } catch (...) {}
    }
    
    return it->second.data;
}

bool Cache::exists(const string& key) {
    size_t shardIdx = getShard(key);
    Shard& shard = *shards[shardIdx];
    
    shared_lock<shared_mutex> lock(shard.mutex);
    
    auto it = shard.store.find(key);
    if (it == shard.store.end()) {
        return false;
    }

    return !it->second.isExpired();
}

bool Cache::del(const string& key) {
    size_t shardIdx = getShard(key);
    Shard& shard = *shards[shardIdx];
    
    unique_lock<shared_mutex> lock(shard.mutex);
    
    if (shard.evictionPolicy) {
        try {
            shard.evictionPolicy->onRemove(key);
        } catch (...) {}
    }
    
    return shard.store.erase(key) > 0;
}

// ============== UTILITY FUNCTIONS ==============

size_t Cache::size() {
    size_t totalSize = 0;
    for (size_t i = 0; i < NUM_SHARDS; i++) {
        shared_lock<shared_mutex> lock(shards[i]->mutex);
        totalSize += shards[i]->store.size();
    }
    return totalSize;
}

void Cache::clear() {
    for (size_t i = 0; i < NUM_SHARDS; i++) {
        unique_lock<shared_mutex> lock(shards[i]->mutex);
        shards[i]->store.clear();
        shards[i]->evictedCount = 0;
        if (shards[i]->evictionPolicy) {
            shards[i]->evictionPolicy->clear();
        }
    }
}

string Cache::evictionPolicy() const {
    switch (evictionPolicyType) {
        case EvictionType::LRU:
            return "LRU";
        case EvictionType::LFU:
            return "LFU";
        default:
            return "NONE";
    }
}

void Cache::enforceCapacityShard(size_t shardIdx) {
    Shard& shard = *shards[shardIdx];
    
    unique_lock<shared_mutex> lock(shard.mutex);
    
    if (!shard.evictionPolicy || shard.shardCapacity == 0) {
        return;
    }
    
    while (shard.store.size() >= shard.shardCapacity) {
        try {
            string keyToEvict = shard.evictionPolicy->evict();
            if (keyToEvict.empty()) {
                break;
            }
            
            auto it = shard.store.find(keyToEvict);
            if (it != shard.store.end()) {
                shard.store.erase(it);
                shard.evictedCount++;
            } else {
                break;
            }
        } catch (...) {
            break;
        }
    }
}

size_t Cache::cleanupExpired() {
    size_t removedCount = 0;
    
    for (size_t i = 0; i < NUM_SHARDS; i++) {
        unique_lock<shared_mutex> lock(shards[i]->mutex);
        
        for (auto it = shards[i]->store.begin(); it != shards[i]->store.end();) {
            if (it->second.isExpired()) {
                if (shards[i]->evictionPolicy) {
                    try {
                        shards[i]->evictionPolicy->onRemove(it->first);
                    } catch (...) {}
                }
                it = shards[i]->store.erase(it);
                removedCount++;
            } else {
                ++it;
            }
        }
    }
    
    return removedCount;
}

void Cache::cleanupExpiredBatch(size_t maxToClean) {
    size_t cleaned = 0;
    
    for (size_t i = 0; i < NUM_SHARDS && cleaned < maxToClean; i++) {
        unique_lock<shared_mutex> lock(shards[i]->mutex);
        
        for (auto it = shards[i]->store.begin(); it != shards[i]->store.end() && cleaned < maxToClean;) {
            if (it->second.isExpired()) {
                if (shards[i]->evictionPolicy) {
                    try {
                        shards[i]->evictionPolicy->onRemove(it->first);
                    } catch (...) {}
                }
                it = shards[i]->store.erase(it);
                cleaned++;
            } else {
                ++it;
            }
        }
    }
}

unordered_map<string, Value> Cache::getAllData() const {
    unordered_map<string, Value> result;
    
    for (size_t i = 0; i < NUM_SHARDS; i++) {
        shared_lock<shared_mutex> lock(shards[i]->mutex);
        for (const auto& pair : shards[i]->store) {
            result[pair.first] = pair.second;
        }
    }
    
    return result;
}

optional<Cache::ValueWithTtl> Cache::getValueWithTtl(const string& key) {
    size_t shardIdx = getShard(key);
    Shard& shard = *shards[shardIdx];
    
    unique_lock<shared_mutex> lock(shard.mutex);
    
    auto it = shard.store.find(key);
    if (it == shard.store.end()) {
        return nullopt;
    }

    if (it->second.isExpired()) {
        // Remove expired key
        if (shard.evictionPolicy) {
            try {
                shard.evictionPolicy->onRemove(key);
            } catch (...) {}
        }
        shard.store.erase(it);
        return nullopt;
    }

    // Track access for eviction policies
    if (shard.evictionPolicy) {
        try {
            shard.evictionPolicy->onAccess(key);
        } catch (...) {}
    }
    
    // Build result with TTL info
    ValueWithTtl result;
    result.data = it->second.data;
    result.hasExpiry = it->second.hasExpiry;
    
    if (it->second.hasExpiry) {
        auto remainingDuration = chrono::duration_cast<chrono::seconds>(
            it->second.expiryTime - chrono::system_clock::now()
        );
        result.remainingTtl = max(0, static_cast<int>(remainingDuration.count()));
    } else {
        result.remainingTtl = nullopt;
    }
    
    return result;
}
