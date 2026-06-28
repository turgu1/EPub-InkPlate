# PageLocs Hardening Quick Reference Guide

## What Was Done

✅ **Hardening Item #1: Correlation IDs**
- Added `uint32_t correlationId` to all queue message structures
- Implemented request/response matching in `PageLocs::retrieveAsap()`
- Detects protocol deviations automatically

✅ **Hardening Item #2: Strict Locking**
- Removed unsafe lock-elision logic from `PageLocs::insert()`
- All pagesMap/itemsSet access now protected by mutex
- Eliminated data races on map operations

✅ **Monitoring & Telemetry**
- Built-in `PageLocs::Telemetry` struct tracks:
  - ASAP request/reply matching
  - Map insertion counts and contentions
  - Protocol deviation indicators
- Accessible at runtime for diagnostics

✅ **Stress Testing Framework**
- 4 concurrent test scenarios
- Automatically detects protocol deviations
- Runnable standalone or in CI

## Files Changed

**Core Implementation:**
- `src/models/page_locs.hpp` - Added correlationId, Telemetry, pending request tracking
- `src/models/page_locs_control.hpp` - Added correlationId to QueueData
- `src/models/page_locs.cpp` - Updated retrieveAsap(), insert() methods

**Testing:**
- `test/test_pagelocs_hardening.cpp` - Complete stress test framework (350+ lines)

**Documentation:**
- `doc/PAGELOCS_HARDENING_ITEMS_1_2_2026-05-12.md` - Full implementation guide
- This guide

## Building the Stress Tests

```bash
cd /home/turgu1/Dev/EPub-InkPlate

# Compile (one time)
g++ -std=c++17 -pthread test/test_pagelocs_hardening.cpp -o build/test_pagelocs_hardening

# Run all stress tests
./build/test_pagelocs_hardening

# Expected output:
# ========================================
# PageLocs Hardening Stress Test Suite
# ========================================
#
# === Test 1: Concurrent Navigation + Percent Queries ===
# Test duration: 3005 ms
# ...
# === Test 4: Map Lock Contention ===
# Lock contention test completed in 2001 ms
#
# === Protocol Compliance Report ===
# ASAP Requests Submitted: 1234
# ASAP Replies Matched: 1234
# ASAP Replies Mismatched (DEVIATION): 0
# ...
# *** PASS: No protocol deviations detected ***
```

## Interpreting Results

**PASS (Green Light):**
```
ASAP Replies Mismatched (DEVIATION): 0
Queue Send Failures (DEVIATION): 0
Correlation ID Mismatches (DEVIATION): 0
*** PASS: No protocol deviations detected ***
```

**FAIL (Red Light):**
```
ASAP Replies Mismatched (DEVIATION): 15        ← Response corruption!
Queue Send Failures (DEVIATION): 8             ← Queue issues!
*** FAIL: 23 protocol deviations detected ***
```

## Telemetry API (Runtime)

Access metrics from your code:

```cpp
#include "models/page_locs.hpp"

// Get current metrics
uint64_t asapRequests = pageLocs.telemetry.asapRequestsSubmitted.load();
uint64_t asapMatched = pageLocs.telemetry.asapRepliesMatched.load();
uint64_t asapMismatched = pageLocs.telemetry.asapRepliesMismatched.load();
uint64_t mapOps = pageLocs.telemetry.mapInsertions.load();
uint64_t contentions = pageLocs.telemetry.mapLockContentions.load();

// Detect deviations
if (asapMismatched > 0) {
  LOG_E("Protocol deviation: {} mismatched ASAP replies", asapMismatched);
}

if (contentions > mapOps / 20) {  // > 5% contention
  LOG_W("High lock contention: {}/{}", contentions, mapOps);
}
```

## Test Scenarios Covered

| Test | Purpose | Stress | Success Criteria |
|------|---------|--------|------------------|
| **Test 1** | Concurrent nav + percent | 4 nav threads, 1 percent thread for 3s | asapRepliesMismatched == 0 |
| **Test 2** | Rapid book switches | 20 STOP/START cycles with overlapping ASAP | stopRequests == stopAcknowledged |
| **Test 3** | Queue saturation | 8 producers, 5-msg queue, 3s | queueSendFailures manageable |
| **Test 4** | Map lock contention | 3 writers, 5 readers, 2s | mapLockContentions < 5% of mapInsertions |

## Backward Compatibility

✅ **No breaking changes:**
- Old code that ignores correlationId continues to work
- Telemetry fields are optional (zero overhead if unused)
- Lock policy strictly more correct, same public API
- Binary compatible (with recompilation)

## Next Steps (Items #3-6)

1. Run stress tests in CI to validate changes
2. Monitor telemetry in production for deviations
3. Implement Item #3: Bounded waits in stopControlTask()
4. Implement Item #4: Queue send failure handling
5. Implement Item #5: Flag synchronization normalization
6. Implement Item #6: Startup/edge-case hardening

## Common Issues & Troubleshooting

**Q: Stress test shows "DEVIATION: Queue full"**
- A: This is intentional in Test 3 (queue saturation scenario)
- Use real pagelocsvs with buffering/retry for production

**Q: High mapLockContentions reported**
- A: Check if many concurrent navigation requests happening
- This is expected behavior; lock is protecting correctness
- Consider two-phase locking if contention exceeds 20%

**Q: Build fails with correlation ID errors**
- A: Recompile all files (not just object files)
- Ensure page_locs.hpp is read by all translation units

## Support

- Full documentation: [PAGELOCS_HARDENING_ITEMS_1_2_2026-05-12.md](doc/PAGELOCS_HARDENING_ITEMS_1_2_2026-05-12.md)
- Interaction analysis: [PAGE_LOCS_INTERACTION_MECHANISM_2026-05-12.md](doc/PAGE_LOCS_INTERACTION_MECHANISM_2026-05-12.md)
- UML diagrams: [doc/uml/page_locs_hardened_*.pu](doc/uml/)
