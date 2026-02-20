#include "cache.h"
#include "distributed_cache.h"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>
#include <atomic>

using namespace std;

mutex mTest;

// ==================== CONCURRENT LOCAL CACHE TEST ====================

/**
 * Test 1: Multiple threads performing concurrent SET/GET operations
 */
void testLocalCacheConcurrency() {
    lock_guard g(mTest);
    cout << "\n" << string(60, '=') << endl;
    cout << "TEST 1: LOCAL CACHE - CONCURRENT SET/GET OPERATIONS" << endl;
    cout << string(60, '=') << endl;

    Cache cache(Cache::EvictionType::LRU, 1000);
    int numThreads = 5;
    int operationsPerThread = 10000;
    atomic<int> successCount(0);
    atomic<int> failureCount(0);

    cout << "\nSpawning " << numThreads << " threads, each performing " 
         << operationsPerThread << " operations..." << endl;

    auto startTime = chrono::high_resolution_clock::now();

    // Lambda function for thread work
    auto threadWork = [&](int threadId) {
        mt19937 gen(threadId);
        uniform_int_distribution<> dis(0, operationsPerThread - 1);
        
        for (int i = 0; i < operationsPerThread; ++i) {
            string key = "key_thread_" + to_string(threadId) + "_" + to_string(i);
            string value = "value_" + to_string(i);
            
            // Set operation
            if (cache.set(key, value)) {
                successCount++;
            } else {
                failureCount++;
            }
            
            // Get operation (read what we just wrote)
            auto result = cache.get(key);
            if (result && result.value() == value) {
                successCount++;
            } else {
                failureCount++;
            }
        }
    };

    // Create and launch threads
    vector<thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(threadWork, i);
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }

    auto endTime = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);

    cout << "  All threads completed!" << endl;
    cout << "  Cache Size: " << cache.size() << endl;
    cout << "  Successful Operations: " << successCount << endl;
    cout << "  Failed Operations: " << failureCount << endl;
    cout << "  Total Time: " << duration.count() << " ms" << endl;
    cout << "  Eviction Policy: " << cache.evictionPolicy() << endl;
}

/**
 * Test 2: Concurrent operations with TTL
 */
void testLocalCacheWithTTL() {
    lock_guard g(mTest);
    cout << "\n" << string(60, '=') << endl;
    cout << "TEST 2: LOCAL CACHE - CONCURRENT SET WITH TTL" << endl;
    cout << string(60, '=') << endl;

    Cache cache(Cache::EvictionType::LFU, 500);
    int numThreads = 4;
    int operationsPerThread = 75;

    cout << "\nSpawning " << numThreads << " threads with TTL operations..." << endl;

    auto startTime = chrono::high_resolution_clock::now();

    auto threadWork = [&](int threadId) {
        for (int i = 0; i < operationsPerThread; ++i) {
            string key = "ttl_key_" + to_string(threadId) + "_" + to_string(i);
            string value = "ttl_value_" + to_string(i);
            int ttl = 5 + (i % 10);  // TTL between 5-15 seconds
            
            cache.setWithTtl(key, value, ttl);
        }
    };

    vector<thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(threadWork, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto endTime = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);

    cout << "  All TTL operations completed!" << endl;
    cout << "  Cache Size: " << cache.size() << endl;
    cout << "  Total Time: " << duration.count() << " ms" << endl;
    cout << "  Eviction Policy: " << cache.evictionPolicy() << endl;
}

// /**
//  * Test 3: Concurrent DEL and EXISTS operations
//  */
void testLocalCacheDeleteAndExists() {
    lock_guard g(mTest);
    cout << "\n" << string(60, '=') << endl;
    cout << "TEST 3: LOCAL CACHE - CONCURRENT DELETE/EXISTS" << endl;
    cout << string(60, '=') << endl;

    Cache cache(Cache::EvictionType::LRU, 2000);
    int numItems = 500;
    
    // Pre-populate cache
    cout << "\nPre-populating cache with " << numItems << " items..." << endl;
    for (int i = 0; i < numItems; ++i) {
        cache.set("item_" + to_string(i), "value_" + to_string(i));
    }

    cout << "Cache has " << cache.size() << " items" << endl;
    cout << "Spawning threads for concurrent DELETE/EXISTS..." << endl;

    atomic<int> deleteCount(0);
    atomic<int> existsCount(0);

    auto startTime = chrono::high_resolution_clock::now();

    auto deleteThread = [&]() {
        for (int i = 0; i < numItems / 2; ++i) {
            if (cache.del("item_" + to_string(i))) {
                deleteCount++;
            }
        }
    };

    auto existsThread = [&]() {
        for (int i = 0; i < numItems; ++i) {
            if (cache.exists("item_" + to_string(i))) {
                existsCount++;
            }
        }
    };

    thread t1(deleteThread);
    thread t2(existsThread);

    t1.join();
    t2.join();

    auto endTime = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);

    cout << "  Concurrent operations completed!" << endl;
    cout << "  Items deleted: " << deleteCount << endl;
    cout << "  Items checked (exists): " << existsCount << endl;
    cout << "  Remaining cache size: " << cache.size() << endl;
    cout << "  Total Time: " << duration.count() << " ms" << endl;
}

// // ==================== CONCURRENT DISTRIBUTED CACHE TEST ====================

// /**
//  * Test 4: Distributed cache with concurrent SET/GET
//  */
void testDistributedCacheConcurrency() {
    lock_guard g(mTest);
    cout << "\n" << string(60, '=') << endl;
    cout << "TEST 4: DISTRIBUTED CACHE - CONCURRENT SET/GET" << endl;
    cout << string(60, '=') << endl;

    DistributedCache cache(150, 500, Cache::EvictionType::LRU, 3);
    int numNodes = 4;
    int numThreads = 8;
    int totalOperations = 100000;
    int operationsPerThread = totalOperations/numThreads;

    // Add nodes to cluster
    cout << "\nAdding " << numNodes << " nodes to cluster..." << endl;
    for (int i = 0; i < numNodes; ++i) {
        cache.addNode("node_" + to_string(i));
    }
    cout << "Cluster ready with nodes: ";
    for (const auto& node : cache.getAllNodes()) {
        cout << node << " ";
    }
    cout << endl;

    atomic<int> successCount(0);
    atomic<int> failureCount(0);

    cout << "\nSpawning " << numThreads << " threads for concurrent operations..." << endl;

    auto startTime = chrono::high_resolution_clock::now();

    auto threadWork = [&](int threadId) {
        for (int i = 0; i < operationsPerThread; ++i) {
            string key = "dist_key_" + to_string(threadId) + "_" + to_string(i);
            string value = "dist_value_" + to_string(i);
            
            if (cache.set(key, value)) {
                successCount++;
            } else {
                failureCount++;
            }
            
            auto result = cache.get(key);
            if (result && result.value() == value) {
                successCount++;
            } else {
                failureCount++;
            }
        }
    };

    vector<thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(threadWork, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto endTime = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);

    cout << "  Distributed cache test completed!" << endl;
    cout << "  Total items in cluster: " << cache.size() << endl;
    cout << "  Successful operations: " << successCount << endl;
    cout << "  Failed operations: " << failureCount << endl;
    cout << "  Total Time: " << duration.count() << " ms" << endl;
    
    cout << "\nNode distribution:" << endl;
    for (const auto& node : cache.getAllNodes()) {
        cout << "  " << node << ": " << cache.getNodeSize(node) << " items" << endl;
    }
}

// /**
//  * Test 5: Distributed cache with node failures
//  */
void testDistributedCacheWithFailures() {
    lock_guard g(mTest);
    cout << "\n" << string(60, '=') << endl;
    cout << "TEST 5: DISTRIBUTED CACHE - FAILURE HANDLING" << endl;
    cout << string(60, '=') << endl;

    DistributedCache cache(150, 300, Cache::EvictionType::LRU, 2);
    int numNodes = 5;

    cout << "\nAdding " << numNodes << " nodes to cluster..." << endl;
    for (int i = 0; i < numNodes; ++i) {
        cache.addNode("node_" + to_string(i));
    }

    cout << "Populating cache..." << endl;
    for (int i = 0; i < 50; ++i) {
        cache.set("key_" + to_string(i), "value_" + to_string(i));
    }

    cout << "Initial cache size: " << cache.size() << " items" << endl;

    cout << "\nThread 1: Simulating node failures..." << endl;
    cout << "Thread 2: Reading data concurrently..." << endl;

    atomic<int> successfulReads(0);
    atomic<int> failedReads(0);

    auto failureThread = [&]() {
        this_thread::sleep_for(chrono::milliseconds(50));
        for (int i = 0; i < 3; ++i) {
            cache.markNodeFailed("node_" + to_string(i));
            cout << "  [Failure Thread] Marked node_" << i << " as failed" << endl;
            this_thread::sleep_for(chrono::milliseconds(100));
        }
    };

    auto readThread = [&]() {
        for (int i = 0; i < 50; ++i) {
            auto result = cache.get("key_" + to_string(i));
            if (result) {
                successfulReads++;
            } else {
                failedReads++;
            }
            this_thread::sleep_for(chrono::milliseconds(10));
        }
    };

    auto startTime = chrono::high_resolution_clock::now();

    thread t1(failureThread);
    thread t2(readThread);

    t1.join();
    t2.join();

    auto endTime = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);

    cout << "  Failure test completed!" << endl;
    cout << "  Successful reads: " << successfulReads << endl;
    cout << "  Failed reads: " << failedReads << endl;
    cout << "  Failed nodes: " << cache.getFailedNodes().size() << endl;
    cout << "  Total Time: " << duration.count() << " ms" << endl;
}

// /**
//  * Test 6: Stress test - high concurrency
//  */
void testStressTest() {
    lock_guard g(mTest);
    cout << "\n" << string(60, '=') << endl;
    cout << "TEST 6: STRESS TEST - HIGH CONCURRENCY" << endl;
    cout << string(60, '=') << endl;

    Cache cache(Cache::EvictionType::LFU, 2000);
    int numThreads = 10;
    int operationsPerThread = 200;
    
    cout << "\nStress testing with " << numThreads << " threads" << endl;
    cout << "Each thread performing " << operationsPerThread << " operations" << endl;

    atomic<int> totalOps(0);
    atomic<int> errors(0);

    auto startTime = chrono::high_resolution_clock::now();

    auto stressWork = [&](int threadId) {
        mt19937 gen(threadId + chrono::system_clock::now().time_since_epoch().count());
        uniform_int_distribution<> opType(0, 2);  // 0: SET, 1: GET, 2: DEL
        
        for (int i = 0; i < operationsPerThread; ++i) {
            string key = "stress_" + to_string(threadId) + "_" + to_string(i);
            string value = "v_" + to_string(i);
            
            int op = opType(gen);
            try {
                switch (op) {
                    case 0:  // SET
                        cache.set(key, value);
                        break;
                    case 1:  // GET
                        cache.get(key);
                        break;
                    case 2:  // DEL
                        cache.del(key);
                        break;
                }
                totalOps++;
            } catch (...) {
                errors++;
            }
        }
    };

    vector<thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(stressWork, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto endTime = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime);
    double throughput = (totalOps.load() * 1000.0) / duration.count();

    cout << "  Stress test completed!" << endl;
    cout << "  Total operations: " << totalOps << endl;
    cout << "  Errors: " << errors << endl;
    cout << "  Cache size: " << cache.size() << endl;
    cout << "  Total Time: " << duration.count() << " ms" << endl;
    cout << "  Throughput: " << fixed << setprecision(2) << throughput << " ops/sec" << endl;
}

int main() {
    cout << "\n" << string(60, '*') << endl;
    cout << "DISTRIBUTED CACHE SYSTEM - CONCURRENT TESTS" << endl;
    cout << string(60, '*') << endl;

    try {
        // Run all tests
        // testLocalCacheConcurrency();
        // testLocalCacheWithTTL();
        // testLocalCacheDeleteAndExists();
        testDistributedCacheConcurrency();
        // testDistributedCacheWithFailures();
        // testStressTest();

        cout << "\n" << string(60, '*') << endl;
        cout << "ALL TESTS COMPLETED SUCCESSFULLY!" << endl;
        cout << string(60, '*') << endl;
    } catch (const exception& e) {
        cerr << "\nERROR: " << e.what() << endl;
        return 1;
    }

    return 0;
}
