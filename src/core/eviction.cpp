#include "eviction.h"
#include <iostream>

using namespace std;

// ============== LRU EVICTION POLICY ==============

void LRUEvictionPolicy::onAccess(const string& key) {
    auto it = keyToIterator.find(key);
    if (it != keyToIterator.end()) {
        // Move to front (most recently used)
        accessOrder.splice(accessOrder.begin(), accessOrder, it->second);
    }
}

void LRUEvictionPolicy::onAdd(const string& key) {
    // Add to front (most recently used)
    accessOrder.push_front(key);
    keyToIterator[key] = accessOrder.begin();
}

void LRUEvictionPolicy::onRemove(const string& key) {
    auto it = keyToIterator.find(key);
    if (it != keyToIterator.end()) {
        accessOrder.erase(it->second);
        keyToIterator.erase(it);
    }
}

string LRUEvictionPolicy::evict() {
    if (accessOrder.empty()) {
        return "";
    }
    
    // Remove from back (least recently used)
    string lruKey = accessOrder.back();
    accessOrder.pop_back();
    keyToIterator.erase(lruKey);
    
    return lruKey;
}

size_t LRUEvictionPolicy::size() const {
    return accessOrder.size();
}

void LRUEvictionPolicy::clear() {
    accessOrder.clear();
    keyToIterator.clear();
}

// ============== LFU EVICTION POLICY ==============

void LFUEvictionPolicy::onAccess(const string& key) {
    auto freqIt = keyFrequency.find(key);
    if (freqIt == keyFrequency.end()) {
        return;
    }
    
    int oldFreq = freqIt->second;
    int newFreq = oldFreq + 1;
    
    // Remove from old frequency bucket
    auto& oldBucket = frequencyBuckets[oldFreq];
    oldBucket.remove(key);
    
    // If old bucket is empty and was minFrequency, increment minFrequency
    if (oldBucket.empty()) {
        frequencyBuckets.erase(oldFreq);
        if (oldFreq == minFrequency) {
            minFrequency = newFreq;
        }
    }
    
    // Add to new frequency bucket
    frequencyBuckets[newFreq].push_back(key);
    keyFrequency[key] = newFreq;
}

void LFUEvictionPolicy::onAdd(const string& key) {
    // New key starts with frequency 1
    keyFrequency[key] = 1;
    frequencyBuckets[1].push_back(key);
    
    // If this is first key, set minFrequency to 1
    if (minFrequency == 0) {
        minFrequency = 1;
    }
}

void LFUEvictionPolicy::onRemove(const string& key) {
    auto freqIt = keyFrequency.find(key);
    if (freqIt == keyFrequency.end()) {
        return;  // Key not found
    }
    
    int freq = freqIt->second;
    frequencyBuckets[freq].remove(key);
    
    if (frequencyBuckets[freq].empty()) {
        frequencyBuckets.erase(freq);
    }
    
    keyFrequency.erase(freqIt);
}

string LFUEvictionPolicy::evict() {
    if (frequencyBuckets.empty() || minFrequency == 0) {
        return "";
    }
    
    // Get least frequently used bucket
    auto& minBucket = frequencyBuckets[minFrequency];
    if (minBucket.empty()) {
        return "";
    }
    
    // Evict first key in min bucket (oldest one due to push_back order)
    string lfuKey = minBucket.front();
    minBucket.pop_front();
    
    // Clean up empty bucket
    if (minBucket.empty()) {
        frequencyBuckets.erase(minFrequency);
    }
    
    keyFrequency.erase(lfuKey);
    
    return lfuKey;
}

size_t LFUEvictionPolicy::size() const {
    return keyFrequency.size();
}

void LFUEvictionPolicy::clear() {
    keyFrequency.clear();
    frequencyBuckets.clear();
    minFrequency = 0;
}
