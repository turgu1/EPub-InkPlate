/**
 * Stress testing framework for PageLocs hardening validation
 * Tests protocol compliance and detects synchronization deviations
 *
 * Build with: g++ -std=c++17 -pthread test_pagelocs_hardening.cpp -o test_pagelocs_hardening
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace PageLocsStressTest {

// Mock structures matching real PageLocs interfaces
struct MockPageLocsMetrics {
  std::atomic<uint64_t> asapRequestsSubmitted{0};
  std::atomic<uint64_t> asapRepliesMatched{0};
  std::atomic<uint64_t> asapRepliesMismatched{0};
  std::atomic<uint64_t> percentRequestsSubmitted{0};
  std::atomic<uint64_t> mapInsertions{0};
  std::atomic<uint64_t> mapLockContentions{0};
  std::atomic<uint64_t> stopRequests{0};
  std::atomic<uint64_t> stopAcknowledged{0};
  std::atomic<uint64_t> queueSendFailures{0};
  std::atomic<uint64_t> correlationIdMismatches{0};

  void reset() {
    asapRequestsSubmitted    = 0;
    asapRepliesMatched       = 0;
    asapRepliesMismatched    = 0;
    percentRequestsSubmitted = 0;
    mapInsertions            = 0;
    mapLockContentions       = 0;
    stopRequests             = 0;
    stopAcknowledged         = 0;
    queueSendFailures        = 0;
    correlationIdMismatches  = 0;
  }

  void report() const {
    std::cout << "\n=== Protocol Compliance Report ===" << std::endl;
    std::cout << "ASAP Requests Submitted: " << asapRequestsSubmitted.load() << std::endl;
    std::cout << "ASAP Replies Matched: " << asapRepliesMatched.load() << std::endl;
    std::cout << "ASAP Replies Mismatched (DEVIATION): " << asapRepliesMismatched.load()
              << std::endl;
    std::cout << "Percent Requests: " << percentRequestsSubmitted.load() << std::endl;
    std::cout << "Map Insertions: " << mapInsertions.load() << std::endl;
    std::cout << "Map Lock Contentions: " << mapLockContentions.load() << std::endl;
    std::cout << "Stop Requests: " << stopRequests.load() << std::endl;
    std::cout << "Stop Acknowledged: " << stopAcknowledged.load() << std::endl;
    std::cout << "Queue Send Failures (DEVIATION): " << queueSendFailures.load() << std::endl;
    std::cout << "Correlation ID Mismatches (DEVIATION): " << correlationIdMismatches.load()
              << std::endl;

    // Summary
    uint64_t deviations =
        asapRepliesMismatched.load() + queueSendFailures.load() + correlationIdMismatches.load();
    if (deviations == 0) {
      std::cout << "\n*** PASS: No protocol deviations detected ***" << std::endl;
    } else {
      std::cout << "\n*** FAIL: " << deviations << " protocol deviations detected ***" << std::endl;
    }
  }
};

// Stress test scenario 1: Concurrent navigation + getPageCountOrPercent
class ConcurrentNavigationTest {
public:
  ConcurrentNavigationTest(int navThreads, int durationSecs, MockPageLocsMetrics &metrics)
      : numNavThreads(navThreads), testDuration(durationSecs), metrics(metrics) {}

  void run() {
    std::cout << "\n=== Test 1: Concurrent Navigation + Percent Queries ===" << std::endl;

    metrics.reset();
    auto startTime = std::chrono::steady_clock::now();
    std::vector<std::thread> threads;

    // Launch navigation threads
    for (int i = 0; i < numNavThreads; i++) {
      threads.emplace_back([this, i]() { navigationWorker(i); });
    }

    // Launch percent query thread
    threads.emplace_back([this]() { percentQueryWorker(); });

    // Wait for test duration
    std::this_thread::sleep_for(std::chrono::seconds(testDuration));
    shouldStop = true;

    for (auto &t : threads) {
      t.join();
    }

    auto elapsed = std::chrono::steady_clock::now() - startTime;
    std::cout << "Test duration: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << " ms"
              << std::endl;
  }

private:
  int numNavThreads;
  int testDuration;
  MockPageLocsMetrics &metrics;
  std::atomic<bool> shouldStop{false};

  void navigationWorker(int threadId) {
    while (!shouldStop) {
      // Simulate GET_ASAP request
      uint32_t requestId = ++correlationCounter;
      metrics.asapRequestsSubmitted.fetch_add(1);

      // Simulate some processing
      std::this_thread::sleep_for(std::chrono::milliseconds(10 + (threadId % 5) * 2));

      // Check if response would match
      if (requestId == correlationCounter) {
        metrics.asapRepliesMatched.fetch_add(1);
      } else {
        metrics.asapRepliesMismatched.fetch_add(1);
        std::cerr << "DEVIATION: Correlation ID mismatch in nav thread " << threadId << std::endl;
      }
    }
  }

  void percentQueryWorker() {
    while (!shouldStop) {
      metrics.percentRequestsSubmitted.fetch_add(1);
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  std::atomic<uint32_t> correlationCounter{0};
};

// Stress test scenario 2: Rapid book switches during ASAP
class RapidBookSwitchTest {
public:
  RapidBookSwitchTest(int switches, MockPageLocsMetrics &metrics)
      : numSwitches(switches), metrics(metrics) {}

  void run() {
    std::cout << "\n=== Test 2: Rapid Book Switches During ASAP Retrieval ===" << std::endl;

    metrics.reset();
    auto startTime = std::chrono::steady_clock::now();

    for (int i = 0; i < numSwitches; i++) {
      // Start a book (ASAP request active)
      uint32_t requestId = ++correlationCounter;
      metrics.asapRequestsSubmitted.fetch_add(1);

      std::thread navThread([this, requestId]() {
        for (int j = 0; j < 5; j++) {
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
          if (requestId == correlationCounter) {
            metrics.asapRepliesMatched.fetch_add(1);
          }
        }
      });

      // Immediately switch (STOP request)
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      metrics.stopRequests.fetch_add(1);

      navThread.join();
      metrics.stopAcknowledged.fetch_add(1);
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    auto elapsed = std::chrono::steady_clock::now() - startTime;
    std::cout << "Completed " << numSwitches << " switches in "
              << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << " ms"
              << std::endl;
  }

private:
  int numSwitches;
  MockPageLocsMetrics &metrics;
  std::atomic<uint32_t> correlationCounter{0};
};

// Stress test scenario 3: Queue saturation
class QueueSaturationTest {
public:
  QueueSaturationTest(int queueDepth, int producers, int durationSecs, MockPageLocsMetrics &metrics)
      : maxQueueDepth(queueDepth), numProducers(producers), testDuration(durationSecs),
        metrics(metrics) {}

  void run() {
    std::cout << "\n=== Test 3: Queue Saturation Detection ===" << std::endl;

    metrics.reset();
    auto startTime = std::chrono::steady_clock::now();
    std::vector<std::thread> threads;

    // Producer threads
    for (int i = 0; i < numProducers; i++) {
      threads.emplace_back([this]() { producer(); });
    }

    // Consumer thread
    threads.emplace_back([this]() { consumer(); });

    std::this_thread::sleep_for(std::chrono::seconds(testDuration));
    shouldStop = true;

    for (auto &t : threads) {
      t.join();
    }

    auto elapsed = std::chrono::steady_clock::now() - startTime;
    std::cout << "Queue saturation test completed in "
              << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << " ms"
              << std::endl;
  }

private:
  struct Message {
    uint32_t correlationId;
    int type; // 0=ASAP, 1=PERCENT
  };

  int maxQueueDepth;
  int numProducers;
  int testDuration;
  MockPageLocsMetrics &metrics;
  std::atomic<bool> shouldStop{false};
  std::deque<Message> queue; // deque-based for capacity tracking
  std::mutex queueMutex;
  std::condition_variable queueCV;

  void producer() {
    int localId = nextProducerId++;
    while (!shouldStop) {
      Message msg{.correlationId = ++correlationCounter, .type = localId % 2};

      {
        std::unique_lock lock(queueMutex);
        if (queue.size() >= maxQueueDepth) {
          metrics.queueSendFailures.fetch_add(1);
          std::cerr << "DEVIATION: Queue full, send failed" << std::endl;
        } else {
          if (msg.type == 0) {
            metrics.asapRequestsSubmitted.fetch_add(1);
          } else {
            metrics.percentRequestsSubmitted.fetch_add(1);
          }
          queue.push_back(msg);
        }
      }
      queueCV.notify_one();
      std::this_thread::sleep_for(std::chrono::milliseconds(1 + (localId % 3)));
    }
  }

  void consumer() {
    while (!shouldStop) {
      std::unique_lock lock(queueMutex);
      queueCV.wait_for(lock, std::chrono::milliseconds(100), [this]() { return !queue.empty(); });

      if (!queue.empty()) {
        Message msg = queue.front();
        queue.pop_front();

        if (msg.type == 0) {
          metrics.asapRepliesMatched.fetch_add(1);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
      }
    }
  }

  std::atomic<uint32_t> correlationCounter{0};
  std::atomic<int> nextProducerId{0};
};

// Stress test scenario 4: Map lock contention
class MapLockContentionTest {
public:
  MapLockContentionTest(int writerThreads, int readerThreads, int durationSecs,
                        MockPageLocsMetrics &metrics)
      : numWriters(writerThreads), numReaders(readerThreads), testDuration(durationSecs),
        metrics(metrics), mapLock(std::make_unique<std::mutex>()) {}

  void run() {
    std::cout << "\n=== Test 4: Map Lock Contention ===" << std::endl;

    metrics.reset();
    auto startTime = std::chrono::steady_clock::now();
    std::vector<std::thread> threads;

    // Writer threads
    for (int i = 0; i < numWriters; i++) {
      threads.emplace_back([this]() { writerWorker(); });
    }

    // Reader threads
    for (int i = 0; i < numReaders; i++) {
      threads.emplace_back([this]() { readerWorker(); });
    }

    std::this_thread::sleep_for(std::chrono::seconds(testDuration));
    shouldStop = true;

    for (auto &t : threads) {
      t.join();
    }

    auto elapsed = std::chrono::steady_clock::now() - startTime;
    std::cout << "Lock contention test completed in "
              << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << " ms"
              << std::endl;
  }

private:
  int numWriters;
  int numReaders;
  int testDuration;
  MockPageLocsMetrics &metrics;
  std::atomic<bool> shouldStop{false};
  std::unique_ptr<std::mutex> mapLock;
  std::map<int, int> mockMap;

  void writerWorker() {
    int insertCount = 0;
    while (!shouldStop) {
      bool contentious = false;
      int attempts     = 0;

      while (true) {
        if (mapLock->try_lock()) {
          mockMap[insertCount++] = insertCount;
          mapLock->unlock();
          metrics.mapInsertions.fetch_add(1);
          if (contentious) {
            metrics.mapLockContentions.fetch_add(1);
          }
          break;
        }
        attempts++;
        contentious = (attempts > 1);
        std::this_thread::sleep_for(std::chrono::microseconds(100));
      }
      std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
  }

  void readerWorker() {
    while (!shouldStop) {
      {
        std::lock_guard lock(*mapLock);
        volatile int size = mockMap.size();
        (void)size;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
};

// Stress test scenario 5: startup ASAP synchronization regression
//
// Reproduces the specific failure mode seen during book open/recompute:
// - Navigation/waiter holds map mutex while waiting for ASAP_READY
// - Retriever needs to insert a page before ASAP_READY can be produced
// - With strict locking and no relax gating, retriever cannot progress
//   and waiter times out.
class StartupAsapSyncRegressionTest {
public:
  bool run() {
    std::cout << "\n=== Test 5: Startup ASAP Synchronization Regression ===" << std::endl;

    bool brokenTimedOut = runCase(/*waiterSetsRelax=*/false,
                                  /*insertUsesRelaxBypass=*/false,
                                  /*waitTimeoutMs=*/150);

    bool fixedSucceeded = runCase(/*waiterSetsRelax=*/true,
                                  /*insertUsesRelaxBypass=*/true,
                                  /*waitTimeoutMs=*/150);

    std::cout << "Broken model timed out (expected): " << (brokenTimedOut ? "yes" : "no")
              << std::endl;
    std::cout << "Fixed model completed (expected): " << (fixedSucceeded ? "yes" : "no")
              << std::endl;

    if (!brokenTimedOut || !fixedSucceeded) {
      std::cerr << "DEVIATION: startup ASAP synchronization regression test failed" << std::endl;
      return false;
    }

    return true;
  }

private:
  std::timed_mutex mapMutex;
  std::mutex cvMutex;
  std::condition_variable cv;
  std::atomic<int> relax{0};
  bool asapReady{false};

  bool runCase(bool waiterSetsRelax, bool insertUsesRelaxBypass, int waitTimeoutMs) {
    asapReady = false;
    relax.store(0);

    std::thread retriever([&]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));

      bool inserted = false;
      if (insertUsesRelaxBypass && (relax.load(std::memory_order_relaxed) > 0)) {
        inserted = true;
      } else if (mapMutex.try_lock_for(std::chrono::milliseconds(30))) {
        mapMutex.unlock();
        inserted = true;
      }

      if (inserted) {
        {
          std::lock_guard<std::mutex> lk(cvMutex);
          asapReady = true;
        }
        cv.notify_one();
      }
    });

    bool gotAsap = false;
    {
      // Mirrors checkAndFind()->retrieveAsap() being called while PageLocs mutex is held.
      std::unique_lock<std::timed_mutex> mapGuard(mapMutex);
      if (waiterSetsRelax) relax.fetch_add(1, std::memory_order_relaxed);

      std::unique_lock<std::mutex> lk(cvMutex);
      gotAsap =
          cv.wait_for(lk, std::chrono::milliseconds(waitTimeoutMs), [&]() { return asapReady; });

      if (waiterSetsRelax) relax.fetch_sub(1, std::memory_order_relaxed);
    }

    retriever.join();

    if (!waiterSetsRelax && !insertUsesRelaxBypass) {
      // In the broken model, timeout is the expected signature.
      return !gotAsap;
    }

    // In the fixed model, ASAP should complete without timeout.
    return gotAsap;
  }
};

// Stress test scenario 6: prev-page fallback must not wrap to tail
//
// Reproduces the reported bug pattern:
// - Current page is first available entry in map (begin)
// - Previous-item ASAP resolution fails/times out
// - Broken fallback wraps to end()-1 (later item currently being processed)
//
// Expected fixed behavior: do not wrap; return "no previous page".
class PrevPageNoForwardWrapRegressionTest {
public:
  bool run() {
    std::cout << "\n=== Test 6: Prev Page No Forward-Wrap Regression ===" << std::endl;

    bool brokenWraps  = runCase(/*useNoWrapFallback=*/false);
    bool fixedNoWraps = runCase(/*useNoWrapFallback=*/true);

    std::cout << "Broken model wraps to tail (expected): " << (brokenWraps ? "yes" : "no")
              << std::endl;
    std::cout << "Fixed model avoids forward wrap (expected): " << (fixedNoWraps ? "yes" : "no")
              << std::endl;

    if (!brokenWraps || !fixedNoWraps) {
      std::cerr << "DEVIATION: prev-page forward-wrap regression test failed" << std::endl;
      return false;
    }

    return true;
  }

private:
  struct PageId {
    int itemrefIndex;
    int offset;
  };

  static bool lessPage(const PageId &a, const PageId &b) {
    if (a.itemrefIndex != b.itemrefIndex) return a.itemrefIndex < b.itemrefIndex;
    return a.offset < b.offset;
  }

  bool runCase(bool useNoWrapFallback) {
    // Simulated computed pages map keys. Current page is begin() entry.
    std::vector<PageId> pages = {
        {15, 0},
        {15, 1200},
        {16, 0},
        {16, 1000},
    };

    std::sort(pages.begin(), pages.end(), lessPage);

    const PageId current{15, 0};
    auto it = std::find_if(pages.begin(), pages.end(), [&](const PageId &p) {
      return p.itemrefIndex == current.itemrefIndex && p.offset == current.offset;
    });
    if (it == pages.end()) {
      std::cerr << "DEVIATION: test setup failed to locate current page" << std::endl;
      return false;
    }

    // Simulate previous-item ASAP timeout/unavailable.
    const bool previousItemResolved = false;

    // "Broken" fallback: if at begin, wrap to tail.
    if (!useNoWrapFallback) {
      if (!previousItemResolved) {
        if (it == pages.begin()) {
          it = pages.end();
        }
        --it;
      }
      // Bug signature: result item is after current item (forward jump).
      return (it->itemrefIndex > current.itemrefIndex);
    }

    // "Fixed" fallback: if unresolved and at begin, no previous page.
    if (!previousItemResolved) {
      if (it == pages.begin()) {
        return true; // expected "no previous" outcome
      }
      --it;
    }

    // If a page is returned, it must not be after current.
    return it->itemrefIndex <= current.itemrefIndex;
  }
};

// Main test orchestrator
class StressTestOrchestrator {
public:
  bool runAllTests() {
    std::cout << "========================================" << std::endl;
    std::cout << "PageLocs Hardening Stress Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;

    bool allPassed = true;

    // Test 1: Concurrent navigation
    {
      ConcurrentNavigationTest test1(4, 3, metrics);
      test1.run();
    }

    // Test 2: Rapid switches
    {
      RapidBookSwitchTest test2(20, metrics);
      test2.run();
    }

    // Test 3: Queue saturation
    {
      QueueSaturationTest test3(5, 8, 3, metrics);
      test3.run();
    }

    // Test 4: Lock contention
    {
      MapLockContentionTest test4(3, 5, 2, metrics);
      test4.run();
    }

    // Test 5: Startup ASAP synchronization regression
    {
      StartupAsapSyncRegressionTest test5;
      if (!test5.run()) allPassed = false;
    }

    // Test 6: Prev-page fallback should never jump forward to map tail
    {
      PrevPageNoForwardWrapRegressionTest test6;
      if (!test6.run()) allPassed = false;
    }

    // Final report
    metrics.report();

    if (!allPassed) {
      std::cout << "\n*** FAIL: Synchronization regression checks failed ***" << std::endl;
      return false;
    }

    return true;
  }

private:
  MockPageLocsMetrics metrics;
};

} // namespace PageLocsStressTest

// Main entry point
int main() {
  PageLocsStressTest::StressTestOrchestrator orchestrator;
  return orchestrator.runAllTests() ? 0 : 1;
}
