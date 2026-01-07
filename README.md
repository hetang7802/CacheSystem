# in-memory Cache System - Phase 1

A high-performance, thread-safe in-memory cache system written in C++17 with features like TTL support, consistent hashing (future phases), and replication (future phases).

## Phase 1: Core Cache Engine

This phase implements the foundation of the cache system with the following features:

### ✅ Implemented Features

- **In-Memory Key-Value Store**: Fast O(1) average-case storage and retrieval
- **Basic CRUD Operations**: SET, GET, DEL, EXISTS
- **TTL (Time-To-Live) Support**: Automatic expiration of keys
- **Thread Safety**: Lock-free reads using `shared_mutex` for high concurrency
- **Expired Key Cleanup**: Manual cleanup of expired keys
- **Simple Command Parser**: Parse and validate user commands

### Data Structures

```
Primary Storage: unordered_map<string, Value>
├── Key: string
└── Value: struct containing
    ├── data: string (actual cached value)
    ├── expiry_time: chrono::system_clock::time_point
    └── has_expiry: bool
```

### Architecture

```
┌─────────────────────────────────┐
│    Interactive CLI (main.cpp)   │
├─────────────────────────────────┤
│    Command Parser               │
│    (validates & parses input)   │
├─────────────────────────────────┤
│    Cache Engine (cache.h/cpp)   │
│    ├─ SET / GET / DEL           │
│    ├─ EXISTS / SIZE             │
│    └─ TTL Handling              │
├─────────────────────────────────┤
│    Thread-Safe Mutex            │
│    (shared_mutex)          │
└─────────────────────────────────┘
```

## Building & Running

### Prerequisites
- CMake 3.10+
- C++17 compatible compiler (MSVC, GCC, Clang)

### Build Instructions

**On Windows (with Visual Studio):**
```bash
cd CacheSystem
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

**On Linux/macOS:**
```bash
cd CacheSystem
mkdir build
cd build
cmake ..
make
```

### Running the Interactive Cache Server

```bash
./cache_server          # On Linux/macOS
./Release/cache_server  # On Windows
```

**Example Usage:**
```
cache> SET user:1 Alice
OK - Set user:1
cache> GET user:1
"Alice"
cache> SET session:abc token123 3600
OK - Set session:abc with TTL 3600s
cache> SIZE
(integer) 2
cache> DEL user:1
(integer) 1
cache> EXISTS user:1
(integer) 0
cache> EXIT
Goodbye!
```

### Running Unit Tests

```bash
ctest --output-on-failure  # On Linux/macOS
ctest --output-on-failure -C Release  # On Windows
```

**Test Coverage (15 tests):**
1. Basic SET/GET operations
2. GET non-existent keys
3. DEL operation
4. EXISTS operation
5. Multiple keys management
6. Value overwriting
7. SIZE operation
8. CLEAR operation
9. TTL expiration
10. EXISTS with TTL
11. Cleanup expired keys
12. Concurrent reads (thread safety)
13. Concurrent writes/reads (thread safety)
14. Large value storage (10KB+)
15. Many keys (1000+ keys)

## API Reference

### Cache Class

```cpp
// Set a key-value pair (no expiry)
void set(const string& key, const string& value);

// Set with TTL
void set_with_ttl(const string& key, const string& value, 
                  int ttl_seconds);

// Get value by key (returns optional<string>)
optional<string> get(const string& key);

// Check if key exists (and is not expired)
bool exists(const string& key);

// Delete a key
bool del(const string& key);

// Get number of non-expired keys
size_t size();

// Clear all keys
void clear();

// Clean up expired keys (returns count removed)
size_t cleanup_expired();
```

### Command Parser Class

```cpp
// Parse a command string
optional<Command> CommandParser::parse(const string& input);

// Validate command structure
bool CommandParser::validate(const Command& cmd);
```

## CLI Commands

| Command | Syntax | Description | Example |
|---------|--------|-------------|---------|
| SET | `SET key value [ttl]` | Store a value (optionally with TTL) | `SET name Alice` |
| GET | `GET key` | Retrieve a value | `GET name` |
| DEL | `DEL key` | Delete a key | `DEL name` |
| EXISTS | `EXISTS key` | Check if key exists | `EXISTS name` |
| SIZE | `SIZE` | Get number of keys | `SIZE` |
| CLEANUP | `CLEANUP` | Remove expired keys | `CLEANUP` |
| CLEAR | `CLEAR` | Clear all data | `CLEAR` |
| HELP | `HELP` | Show help message | `HELP` |
| EXIT | `EXIT` | Exit program | `EXIT` |

## Performance Characteristics

### Time Complexity
- **SET**: O(1) average case
- **GET**: O(1) average case (O(n) worst case for expiry check)
- **DEL**: O(1) average case
- **EXISTS**: O(1) average case
- **CLEANUP**: O(n) where n = total keys

### Space Complexity
- O(n) where n = number of stored keys

### Concurrency
- Multiple readers: Supported (uses `shared_lock`)
- Multiple writers: Supported (uses `unique_lock`)
- Readers & writers together: Supported (safe)

## Key Implementation Details

### TTL Handling
- TTL is checked on `GET` and `EXISTS` operations
- Expired keys are lazily deleted on access
- `cleanup_expired()` performs batch removal of expired keys
- Prevents memory bloat from stale data

### Thread Safety
- Uses `shared_mutex` for reader-writer locking
- Multiple readers can access simultaneously
- Writers get exclusive access
- Reader-writer mixes are safe (lock upgrade on expiry)

### Example: Lazy Expiration
```cpp
// When you GET a key, if expired:
1. Read lock acquired
2. Key found but is_expired() returns true
3. Release read lock, acquire write lock
4. Remove from map
5. Return null
```

## Future Phases

- **Phase 2**: LRU/LFU eviction policies for memory management
- **Phase 3**: Consistent hashing for distributed caching
- **Phase 4**: Master-Slave replication for fault tolerance
- **Phase 5**: TCP networking and Redis protocol support
- **Phase 6**: Persistence (AOF, snapshots)

## File Structure

```
CacheSystem/
├── include/
│   ├── cache.h                 # Main cache interface
│   └── command_parser.h        # Command parsing
├── src/
│   ├── core/
│   │   ├── cache.cpp           # Cache implementation
│   │   └── command_parser.cpp  # Parser implementation
│   └── main.cpp                # Interactive CLI
├── tests/
│   └── test_cache.cpp          # Unit tests
├── CMakeLists.txt
└── README.md
```

## Code Quality

- **Lines of Code**: ~600 (core) + 400 (tests)
- **Test Coverage**: 15 comprehensive unit tests
- **Thread Safety**: Verified with concurrent test cases
- **Memory Safety**: No manual memory management (RAII)
- **Comments**: Well-documented with usage examples

## Resume Highlights

This project demonstrates:
- ✅ Advanced C++ features (smart pointers, templates, thread-safe data structures)
- ✅ System design (in-memory storage, TTL, expiration)
- ✅ Concurrency (shared_mutex, reader-writer patterns)
- ✅ Data structures (hash maps, lists for future LRU)
- ✅ Software engineering (testing, documentation, modularity)
- ✅ Performance optimization (O(1) operations, lazy expiration)

## License

MIT License - Feel free to use for learning and portfolio purposes.

## Next Steps

1. **Test locally** - Build and run `cache_server` to verify setup
2. **Review tests** - Run unit tests and study the test patterns
3. **Extend Phase 1** - Add persistence, statistics, metrics
4. **Move to Phase 2** - Implement LRU/LFU eviction policies
5. **Deploy** - Add networking and make it a real service

---

**Created**: January 2026  
**Status**: Phase 1 - Core Engine ✓
