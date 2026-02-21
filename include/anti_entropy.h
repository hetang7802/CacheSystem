#ifndef ANTI_ENTROPY_H
#define ANTI_ENTROPY_H

#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <memory>
#include <shared_mutex>
#include <condition_variable>

using namespace std;

class DistributedCache;

class AntiEntropyService {
public:
    struct RepairStats {
        size_t totalKeysScanned = 0;
        size_t underReplicatedKeys = 0;
        size_t keysRepaired = 0;
        size_t repairFailures = 0;
        chrono::system_clock::time_point lastRunTime;
        chrono::milliseconds lastRunDuration{0};
    };

    AntiEntropyService(DistributedCache* cache, 
                       chrono::seconds repairInterval = chrono::seconds(60));
    ~AntiEntropyService();

    void start();
    void stop();
    bool isRunning() const { return running; }
    
    // Manual trigger for repair
    void triggerRepair();
    
    RepairStats getStats() const;

private:
    void repairLoop();
    void performRepair();
    
    DistributedCache* cache;
    chrono::seconds repairInterval;
    
    atomic<bool> running{false};
    atomic<bool> stopRequested{false};
    thread repairThread;
    
    mutable shared_mutex statsMutex;
    RepairStats stats;
    mutex stopMutex;
    condition_variable stopCV;
};

#endif // ANTI_ENTROPY_H