#include "cache.h"
#include <algorithm>
#include <mutex>
#include <iostream>

using namespace std;

Cache::Cache() 
    : maxCapacity(0), evictedCount(0), evictionPolicyImpl(nullptr) {}

Cache::Cache(EvictionType policyType, size_t maxCapacityVal)
    : maxCapacity(maxCapacityVal), evictedCount(0) {
    switch (policyType) {
        case EvictionType::LRU:
            evictionPolicyImpl = make_unique<LRUEvictionPolicy>();
            break;
        case EvictionType::LFU:
            evictionPolicyImpl = make_unique<LFUEvictionPolicy>();
            break;
        default:
            evictionPolicyImpl = nullptr;
    }
}

Cache::~Cache() {}

bool Cache::set(const string& key, const string& value) {
    unique_lock<shared_mutex> lock(mutex);
    
    // If key exists, we're updating (not adding)
    bool keyExists = store.find(key) != store.end();
    
    if (!keyExists && maxCapacity > 0 && store.size() >= maxCapacity) {
        enforceCapacity();
        
        // If still full after eviction, fail
        if (store.size() >= maxCapacity) {
            return false;
        }
    }
    
    // If updating existing key, tell eviction policy
    if (keyExists) {
        if (evictionPolicyImpl) {
            evictionPolicyImpl->onAccess(key);
        }
    } else {
        // New key
        if (evictionPolicyImpl) {
            evictionPolicyImpl->onAdd(key);
        }
    }
    
    store[key] = Value(value);
    return true;
}

bool Cache::setWithTtl(const string& key, const string& value, 
                         int ttlSeconds) {
    unique_lock<shared_mutex> lock(mutex);
    
    bool keyExists = store.find(key) != store.end();
    
    if (!keyExists && maxCapacity > 0 && store.size() >= maxCapacity) {
        enforceCapacity();
        
        if (store.size() >= maxCapacity) {
            return false;
        }
    }
    
    if (keyExists) {
        if (evictionPolicyImpl) {
            evictionPolicyImpl->onAccess(key);
        }
    } else {
        if (evictionPolicyImpl) {
            evictionPolicyImpl->onAdd(key);
        }
    }
    
    auto expiryTime = chrono::system_clock::now() + 
                      chrono::seconds(ttlSeconds);
    store[key] = Value(value, expiryTime);
    return true;
}

optional<string> Cache::get(const string& key) {
    unique_lock<shared_mutex> lock(mutex);
    
    auto it = store.find(key);
    if (it == store.end()) {
        return nullopt;
    }

    if (it->second.isExpired()) {
        auto it2 = store.find(key);
        if (it2 != store.end() && it2->second.isExpired()) {
            if (evictionPolicyImpl) {
                evictionPolicyImpl->onRemove(key);
            }
            store.erase(it2);
        }
        return nullopt;
    }

    // Track access for eviction policies
    if (evictionPolicyImpl) {
        evictionPolicyImpl->onAccess(key);
    }
    
    return it->second.data;
}

bool Cache::exists(const string& key) {
    unique_lock<shared_mutex> lock(mutex);
    
    auto it = store.find(key);
    if (it == store.end()) {
        return false;
    }

    if (it->second.isExpired()) {
        auto it2 = store.find(key);
        if (it2 != store.end() && it2->second.isExpired()) {
            if (evictionPolicyImpl) {
                evictionPolicyImpl->onRemove(key);
            }
            store.erase(it2);
        }
        return false;
    }

    return true;
}

bool Cache::del(const string& key) {
    unique_lock<shared_mutex> lock(mutex);
    if (evictionPolicyImpl) {
        evictionPolicyImpl->onRemove(key);
    }
    return store.erase(key) > 0;
}

// todo : iterating 
size_t Cache::size() {
    // Return approximate size without locking or iterating
    // This is fast but may be slightly inaccurate due to undetected expired items
    return approximateSize.load();
}

void Cache::clear() {
    unique_lock<shared_mutex> lock(mutex);
    if (evictionPolicyImpl) {
        evictionPolicyImpl->clear();
    }
    store.clear();
    evictedCount = 0;
}

string Cache::evictionPolicy() const {
    if (!evictionPolicyImpl) {
        return "NONE";
    }
    return evictionPolicyImpl->policyName();
}

void Cache::enforceCapacity() {
    // Must be called with lock held
    if (!evictionPolicyImpl || maxCapacity == 0) {
        return;
    }
    
    while (store.size() >= maxCapacity) {
        string keyToEvict = evictionPolicyImpl->evict();
        if (keyToEvict.empty()) {
            break;  // Nothing more to evict
        }
        
        auto it = store.find(keyToEvict);
        if (it != store.end()) {
            store.erase(it);
            evictedCount++;
        }
    }
}

// todo : only called by cli should be removed
size_t Cache::cleanupExpired() {
    unique_lock<shared_mutex> lock(mutex);
    size_t removedCount = 0;
    
    for (auto it = store.begin(); it != store.end();) {
        if (it->second.isExpired()) {
            if (evictionPolicyImpl) {
                evictionPolicyImpl->onRemove(it->first);
            }
            it = store.erase(it);
            removedCount++;
        } else {
            ++it;
        }
    }
    
    return removedCount;
}

unordered_map<string, Value> Cache::getAllData() const {
    shared_lock<shared_mutex> lock(mutex);
    
    return store;
}

bool Cache::removeIfExpired(const string& key) {
    auto it = store.find(key);
    if (it != store.end() && it->second.isExpired()) {
        store.erase(it);
        return true;
    }
    return false;
}

void Cache::cleanupExpiredBatch(size_t maxToClean) {
    // Must be called with lock held
    size_t cleaned = 0;
    
    for (auto it = store.begin(); it != store.end() && cleaned < maxToClean;) {
        if (it->second.isExpired()) {
            if (evictionPolicyImpl) {
                evictionPolicyImpl->onRemove(it->first);
            }
            it = store.erase(it);
            approximateSize--;
            cleaned++;
        } else {
            ++it;
        }
    }
}

void Cache::onExpiredFound() {
    // Track expired encounters
    size_t expired = expiredEncountered.fetch_add(1) + 1;
    
    // If we've encountered enough expired items, trigger batch cleanup
    if (expired >= EXPIRY_THRESHOLD) {
        expiredEncountered = 0;  // Reset counter
        cleanupExpiredBatch(BATCH_CLEANUP_SIZE);
    }
}