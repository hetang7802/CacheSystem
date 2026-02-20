#include "anti_entropy.h"
#include "distributed_cache.h"
#include <iostream>
#include <math.h>

AntiEntropyService::AntiEntropyService(DistributedCache* cache,
                                       chrono::seconds repairInterval)
    : cache(cache), repairInterval(repairInterval) {}

AntiEntropyService::~AntiEntropyService() {
    stop();
}

void AntiEntropyService::start() {
    bool expected = false;
    if (!running.compare_exchange_strong(expected, true)) {
        return;
    }
    cout << "starting repair service" << endl;
    
    stopRequested = false;
    repairThread = thread(&AntiEntropyService::repairLoop, this);
}

void AntiEntropyService::stop() {
    bool expected = true;
    if (!running.compare_exchange_strong(expected, false)) {
        return;
    }

    cout << "marking to stop repair service " << endl;
    
    stopRequested = true;
    stopCV.notify_one();
    repairThread.join();
}

void AntiEntropyService::repairLoop() {
    while (!stopRequested) {
        auto startTime = chrono::system_clock::now();
        
        performRepair();
        
        auto endTime = chrono::system_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);
        
        {
            unique_lock<shared_mutex> lock(statsMutex);
            stats.lastRunTime = endTime;
            stats.lastRunDuration = duration;
        }
        
        // Sleep until next repair cycle
        unique_lock<mutex> lock(stopMutex);
        stopCV.wait_for(lock, repairInterval, [this]() {
            return stopRequested.load();
        });
    }
}

void AntiEntropyService::triggerRepair() {
    performRepair();
}

void AntiEntropyService::performRepair() {
    // Get all nodes in the cluster
    auto allNodes = cache->getAllHealthyNodes();
    
    size_t keysScanned = 0;
    size_t underReplicated = 0;
    size_t repaired = 0;
    size_t failures = 0;
    
    // Scan all nodes and collect unique keys
    unordered_set<string> allKeys;
    
    vector<string> failedNodes = cache->getFailedNodes();
    for (const auto& nodeId : allNodes) {
        auto keys = cache->getKeysOnNode(nodeId);
        for (const auto& key : keys) {
            allKeys.insert(key);
        }
    }
    
    // Check each key's replication count
    for (const auto& key : allKeys) {
        keysScanned++;
        
        size_t actualCount = cache->getActualReplicationCount(key);
        size_t expectedCount = max(cache->getReplicationFactor(), cache->getAllHealthyNodes().size());
        
        if (actualCount < expectedCount) {
            underReplicated++;
            
            // Try to repair
            cout << "for key " << key << " "<< actualCount << " " << expectedCount << endl;
            if (cache->repairKey(key)) {
                repaired++;
            } else {
                failures++;
            }
        }
    }
    
    // Update stats
    unique_lock<shared_mutex> lock(statsMutex);
    stats.totalKeysScanned = keysScanned;
    stats.underReplicatedKeys = underReplicated;
    stats.keysRepaired = repaired;
    stats.repairFailures = failures;
}

AntiEntropyService::RepairStats AntiEntropyService::getStats() const {
    shared_lock<shared_mutex> lock(statsMutex);
    return stats;
}