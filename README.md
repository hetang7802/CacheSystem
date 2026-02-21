# Distributed in-memory Cache System 

### To test working 
g++ -std=c++17 -pthread -g src/concurrent_test.cpp src/core/*.cpp -I./include -o concurrent_test

### sample test output
**********************************************************
DISTRIBUTED CACHE SYSTEM - CONCURRENT TESTS
************************************************************

============================================================
TEST 1: LOCAL CACHE - CONCURRENT SET/GET OPERATIONS
============================================================

Spawning 5 threads, each performing 10000 operations...
  All threads completed!
  Cache Size: 992
  Successful Operations: 100000
  Failed Operations: 0
  Total Time: 64 ms
  Eviction Policy: LRU

============================================================
TEST 2: LOCAL CACHE - CONCURRENT SET WITH TTL
============================================================

Spawning 4 threads with TTL operations...
  All TTL operations completed!
  Cache Size: 295
  Total Time: 0 ms
  Eviction Policy: LFU

============================================================
TEST 3: LOCAL CACHE - CONCURRENT DELETE/EXISTS
============================================================

Pre-populating cache with 500 items...
Cache has 500 items
Spawning threads for concurrent DELETE/EXISTS...
  Concurrent operations completed!
  Items deleted: 250
  Items checked (exists): 250
  Remaining cache size: 250
  Total Time: 0 ms

============================================================
TEST 4: DISTRIBUTED CACHE - CONCURRENT SET/GET
============================================================
starting repair service

Adding 4 nodes to cluster...
Cluster ready with nodes: node_0 node_1 node_2 node_3

Spawning 8 threads for concurrent operations...
  Distributed cache test completed!
  Total items in cluster: 1984
  Successful operations: 200000
  Failed operations: 0
  Total Time: 2777 ms

Node distribution:
  node_0: 496 items
  node_1: 496 items
  node_2: 496 items
  node_3: 496 items
marking to stop repair service 

============================================================
TEST 5: DISTRIBUTED CACHE - FAILURE HANDLING
============================================================
starting repair service

Adding 5 nodes to cluster...
Populating cache...
Initial cache size: 100 items

Thread 1: Simulating node failures...
Thread 2: Reading data concurrently...
  [Failure Thread] Marked node_0 as failed
  [Failure Thread] Marked node_1 as failed
  [Failure Thread] Marked node_2 as failed
  Failure test completed!
  Successful reads: 43
  Failed reads: 7
  Failed nodes: 3
  Total Time: 540 ms
marking to stop repair service

============================================================
TEST 4: DISTRIBUTED CACHE - CONCURRENT SET/GET
============================================================
starting repair service

Adding 10 nodes to cluster...
Cluster ready with nodes: node_0 node_1 node_2 node_3 node_4 node_5 node_6 node_7 node_8 node_9 

Spawning 5 threads for concurrent operations...
  Distributed cache test completed!
  Total items in cluster: 4960
  Successful operations: 200000
  Failed operations: 0
  Total Time: 3079 ms

Node distribution:
  node_0: 496 items
  node_1: 496 items
  node_2: 496 items
  node_3: 496 items
  node_4: 496 items
  node_5: 496 items
  node_6: 496 items
  node_7: 496 items
  node_8: 496 items
  node_9: 496 items
marking to stop repair service

============================================================
TEST 7: ANTI-ENTROPY SERVICE - DATA REPAIR
============================================================
starting repair service

Adding 5 nodes to cluster...
Populating cache with 100 items (replication factor: 3)...
Initial cache size: 300 items

Simulating failures on 2 nodes to create under-replicated data...
  Marked node_0 and node_1 as failed
  Failed nodes: 2

Starting Anti-Entropy Service for automatic repair...
  Waiting for repair cycles to complete 35  seconds)...
reparing key repair_key_73
reparing key repair_key_81
reparing key repair_key_36
reparing key repair_key_65
reparing key repair_key_16
reparing key repair_key_98
reparing key repair_key_35
reparing key repair_key_24
reparing key repair_key_97
reparing key repair_key_21
reparing key repair_key_41
reparing key repair_key_13
reparing key repair_key_11
reparing key repair_key_32
reparing key repair_key_28
reparing key repair_key_20
reparing key repair_key_74
reparing key repair_key_93
reparing key repair_key_84
reparing key repair_key_64
reparing key repair_key_22
reparing key repair_key_49
reparing key repair_key_99
reparing key repair_key_8
reparing key repair_key_78
reparing key repair_key_61
reparing key repair_key_92
reparing key repair_key_96
reparing key repair_key_14
reparing key repair_key_69
reparing key repair_key_56
reparing key repair_key_30
reparing key repair_key_53
reparing key repair_key_90
reparing key repair_key_52
reparing key repair_key_1
reparing key repair_key_59
reparing key repair_key_75
reparing key repair_key_6
reparing key repair_key_10
reparing key repair_key_87
reparing key repair_key_37
reparing key repair_key_83
reparing key repair_key_29
reparing key repair_key_33
reparing key repair_key_9
reparing key repair_key_15
reparing key repair_key_2
reparing key repair_key_31
reparing key repair_key_42
reparing key repair_key_17
reparing key repair_key_76
reparing key repair_key_77
reparing key repair_key_18
reparing key repair_key_23
reparing key repair_key_27
reparing key repair_key_89
reparing key repair_key_54
reparing key repair_key_68
reparing key repair_key_85
reparing key repair_key_55
reparing key repair_key_7
reparing key repair_key_34
reparing key repair_key_40
reparing key repair_key_25
reparing key repair_key_51
reparing key repair_key_43
reparing key repair_key_94
reparing key repair_key_50
reparing key repair_key_66
reparing key repair_key_86
reparing key repair_key_95
reparing key repair_key_46
reparing key repair_key_48
reparing key repair_key_62
reparing key repair_key_60
reparing key repair_key_0
reparing key repair_key_26
reparing key repair_key_45
reparing key repair_key_12
reparing key repair_key_3
reparing key repair_key_57
reparing key repair_key_88
reparing key repair_key_67
reparing key repair_key_71
reparing key repair_key_4
reparing key repair_key_39
reparing key repair_key_82
reparing key repair_key_63
reparing key repair_key_47
reparing key repair_key_79
reparing key repair_key_91
reparing key repair_key_19
reparing key repair_key_44
reparing key repair_key_5
reparing key repair_key_70
reparing key repair_key_58
reparing key repair_key_80
reparing key repair_key_38
reparing key repair_key_72

Repair cycle completed!
  Total Time: 34293 ms

Verifying data integrity AFTER repair...
  Keys accessible after repair:  100/100
  Keys lost: 0/100
  Cache size after repair: 421 items

Node state after repair:
  node_0: 60 items [FAILED]
  node_1: 61 items [FAILED]
  node_2: 100 items [HEALTHY]
  node_3: 100 items [HEALTHY]
  node_4: 100 items [HEALTHY]
  Anti-Entropy Service test completed!
marking to stop repair service

************************************************************
ALL TESTS COMPLETED SUCCESSFULLY!
************************************************************
