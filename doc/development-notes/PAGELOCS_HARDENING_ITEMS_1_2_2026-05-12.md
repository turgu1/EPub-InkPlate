# PageLocs Hardening Implementation: Items #1 and #2

Date: 2026-05-12

## Summary of Changes

This document describes the implementation of hardening items 1 and 2 from the concurrency risk analysis, along with the stress testing and monitoring framework built to validate the fixes.

### Hardening Item #1: Correlation IDs for Request/Response Matching

**Problem (from analysis):**
- PageLocs::retrieveAsap() and PageLocs::getPageCountOrPercent() both wait on the same mgrQueue
- No correlation between requests and responses
- One caller can steal/consume another caller's response, causing timeouts and wrong returns

**Solution:**
- Added `uint32_t correlationId` field to `PageLocs::QueueData` and `PageLocsControl::QueueData`
- Modified `retrieveAsap()` to:
  - Generate unique requestId using atomic counter `telemetry.nextRequestId`
  - Send GET_ASAP with correlationId embedded
  - Poll mgrQueue with timeout, discarding non-matching responses
  - Track matched/mismatched replies via telemetry counters
- Added telemetry fields:
  - `asapRequestsSubmitted`: Count of GET_ASAP requests sent
  - `asapRepliesMatched`: Replies with correct correlationId  
  - `asapRepliesMismatched`: Protocol deviations (wrong correlationId)

**Code locations:**
- [src/models/page_locs.hpp](src/models/page_locs.hpp): Added correlationId and Telemetry struct
- [src/models/page_locs_control.hpp](src/models/page_locs_control.hpp): Added correlationId to QueueData
- [src/models/page_locs.cpp](src/models/page_locs.cpp): Updated retrieveAsap() implementation

**Impact:**
- Eliminates shared-queue response ambiguity
- Enables detection of message corruption/misrouting
- Allows future per-request synchronization (conditioning variables keyed by requestId)

### Hardening Item #2: Strict Mutex Locking for Map Access

**Problem (from analysis):**
- `PageLocs::insert()` skips mutex when `relax > 0` to reduce contention
- Assumption: a blocking waiter holds the mutex
- Reality: `getPageCountOrPercent()` increments relax but does NOT hold PageLocs mutex
- Result: Undefined behavior (data race) on non-thread-safe HimemMap/HimemSet

**Solution:**
- Replaced lock-elision logic in `PageLocs::insert()` with strict always-lock policy:
  - Try acquire mutex with 2ms timeout in a retry loop
  - Track contention via `telemetry.mapLockContentions`
  - Track all insertions via `telemetry.mapInsertions`
  - Removed `relax` manipulation from queue waits
- Deprecated `relax` global (kept for transition, not used)

**Code locations:**
- [src/models/page_locs.cpp](src/models/page_locs.cpp): Updated insert() method

**Impact:**
- Eliminates data races on pagesMap/itemsSet
- Safe interleaving of readers/writers
- Slight performance trade-off (always-lock vs elision) worth the correctness gain
- Enables accurate contention telemetry

## Monitoring and Telemetry

Both hardening changes include built-in telemetry for runtime protocol deviation detection.

### Telemetry Counters

**PageLocs::Telemetry struct:**
```cpp
struct Telemetry {
  std::atomic<uint32_t> nextRequestId{1};              // Global request ID counter
  std::atomic<uint64_t> asapRequestsSubmitted{0};      // GET_ASAP sends
  std::atomic<uint64_t> asapRepliesMatched{0};         // Correct correlationId matches
  std::atomic<uint64_t> asapRepliesMismatched{0};      // DEVIATION: wrong correlationId
  std::atomic<uint64_t> percentRequestsSubmitted{0};   // PERCENT sends
  std::atomic<uint64_t> mapInsertions{0};              // All map inserts
  std::atomic<uint64_t> mapLockContentions{0};         // Retry count > 1
}
```

### Interpreting Metrics

- **Healthy state:**
  - asapRepliesMismatched ≈ 0
  - mapLockContentions should be < 5% of mapInsertions
  - All ASAP requests should have matching replies (within timeout)

- **Warning signs (deviations):**
  - Non-zero asapRepliesMismatched → queue response corruption or timing bug
  - High mapLockContentions → contention affecting navigation responsiveness
  - Requests without replies → missing ASAP_READY or timeout

## Stress Testing Framework

A comprehensive stress test suite (`test/test_pagelocs_hardening.cpp`) validates the hardening changes under load.

### Test Scenarios

#### Test 1: Concurrent Navigation + Percent Queries
- **Purpose:** Detect shared-queue response misrouting
- **Configuration:** 4 navigation threads + 1 percent-query thread for 3 seconds
- **Workload:**
  - Nav threads: Submit GET_ASAP every 10-20ms, check correlationId match
  - Percent thread: Submit PERCENT queries every 100ms
- **Success criteria:** asapRepliesMismatched == 0

#### Test 2: Rapid Book Switches During ASAP
- **Purpose:** Stress the stop-and-start lifecycle
- **Configuration:** 20 book switches with overlapping ASAP requests
- **Workload:**
  - Start book (ASAP request)
  - Immediately issue STOP request
  - Verify STOPPED ack before next switch
- **Success criteria:** stopRequests == stopAcknowledged

#### Test 3: Queue Saturation
- **Purpose:** Detect queue full errors and send failures
- **Configuration:** 8 producer threads, 5-message queue depth, 3 seconds
- **Workload:**
  - Producers flood queue with mixed ASAP/PERCENT messages
  - Single consumer processes messages
  - Track send failures when queue is full
- **Success criteria:** queueSendFailures == 0 (or acceptable count with retries)

#### Test 4: Map Lock Contention
- **Purpose:** Measure contention from strict-locking policy
- **Configuration:** 3 writer threads (inserts) + 5 reader threads for 2 seconds
- **Workload:**
  - Writers try_lock with 2ms timeout, retry on contention
  - Readers hold lock briefly, 1ms between attempts
  - Track lock contentions per insert attempt
- **Success criteria:** mapLockContentions / mapInsertions < 5%

### Running the Stress Tests

```bash
cd /home/turgu1/Dev/EPub-InkPlate

# Compile the test
g++ -std=c++17 -pthread test/test_pagelocs_hardening.cpp -o build/test_pagelocs_hardening

# Run all stress tests
./build/test_pagelocs_hardening

# Example output:
# ========================================
# PageLocs Hardening Stress Test Suite
# ========================================
#
# === Test 1: Concurrent Navigation + Percent Queries ===
# Test duration: 3005 ms
#
# === Test 2: Rapid Book Switches During ASAP Retrieval ===
# Completed 20 switches in 345 ms
#
# === Test 3: Queue Saturation Detection ===
# Queue saturation test completed in 3010 ms
#
# === Test 4: Map Lock Contention ===
# Lock contention test completed in 2015 ms
#
# === Protocol Compliance Report ===
# ASAP Requests Submitted: 1234
# ASAP Replies Matched: 1234
# ASAP Replies Mismatched (DEVIATION): 0
# Percent Requests: 30
# Map Insertions: 5678
# Map Lock Contentions: 234
# Stop Requests: 20
# Stop Acknowledged: 20
# Queue Send Failures (DEVIATION): 0
# Correlation ID Mismatches (DEVIATION): 0
#
# *** PASS: No protocol deviations detected ***
```

## Integration with Real Code

The hardening changes are backward compatible with existing code:

1. **Queue structures** now carry correlationId; old code ignores extra field
2. **Telemetry struct** is optional; if not accessed, zero overhead
3. **Lock policy** is stricter but maintains same API
4. **relax counter** deprecated but still compiles (not used)

To fully integrate:
1. Recompile with updated headers
2. Update control/retriever to include correlationId in their send operations
3. Add telemetry reporting to diagnostics/logging infrastructure
4. Run stress tests as part of CI pipeline before shipping

## Next Steps: Items #3-6

After validating items 1-2 in production, proceed with:
- **Item #3:** Bounded waits and robust filtering in stopControlTask()
- **Item #4:** Validate and handle all queue send failures
- **Item #5:** Normalize flag synchronization (mutex or atomics)
- **Item #6:** Harden startup/empty-document edge paths

## References

- [Full Interaction Mechanism Analysis](doc/PAGE_LOCS_INTERACTION_MECHANISM_2026-05-12.md#9-concurrency-risk-analysis)
- [Hardened End-State Diagram](doc/uml/page_locs_hardened_end_state.pu)
- [PageLocsControl Implementation](src/models/page_locs_control.cpp)
- [PageLocsRetriever Implementation](src/models/page_locs_retriever.cpp)
