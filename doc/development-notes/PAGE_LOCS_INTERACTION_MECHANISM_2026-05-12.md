# Interaction mechanism: BookController, PageLocs, PageLocsControl, PageLocsRetriever

Date: 2026-05-12

This document describes the current runtime interaction between:
- BookController (UI/navigation orchestrator)
- PageLocs (page location manager and shared map owner)
- PageLocsControl (scheduling/control thread)
- PageLocsRetriever (page boundary computation thread)

It reflects the code paths in:
- src/controllers/book_controller.cpp
- src/models/page_locs.cpp
- src/models/page_locs_control.cpp
- src/models/page_locs_retriever.cpp

## 1. Responsibilities and ownership

BookController:
- Calls into PageLocs when opening a book, entering the viewer, and navigating pages.
- Does not communicate directly with PageLocsControl or PageLocsRetriever.

PageLocs:
- Owns the page index structures (pagesMap, itemsSet) and exposes lookup APIs.
- Owns and lifecycle-manages a PageLocsControl instance.
- Receives asynchronous status messages via mgrQueue (ASAP_READY, PERCENT, STOPPED).
- Handles persistence of computed locations (.locs load/save).

PageLocsControl:
- Owns one PageLocsRetriever instance.
- Receives manager commands (START_DOCUMENT, GET_ASAP, STOP, PERCENT).
- Schedules item retrieval order and tracks done items through a bitset.
- Sends manager-facing notifications through PageLocs::send().

PageLocsRetriever:
- Computes page boundaries for a given spine item.
- Inserts computed PageId/PageInfo entries into PageLocs::pagesMap via pageLocs.insert().
- Can abort current work at page boundaries when ASAP or STOP preemption is requested.
- Reports completion/interruption back to PageLocsControl.

## 2. Startup and new-document flow

1. BookController::openBook() calls pageLocs.stopControlTask() before switching documents.
2. BookController creates/opens EPub, then calls pageLocs.startNewDocument(epub, itemrefIndex).
3. PageLocs::startNewDocument() stores current filename and calls checkForFormatChanges(...).
4. If recomputation is required (forced, format mismatch, or no TOC/.locs):
   - PageLocs stops any running control task.
   - PageLocs clears maps/state and resets computation flags.
   - PageLocs creates PageLocsControl and calls controlTask->setup(epubFilename).
   - PageLocsControl sets up PageLocsRetriever and starts its own control thread.
   - PageLocs sends START_DOCUMENT(itemrefIndex, itemCount) to PageLocsControl.
5. PageLocsControl initializes scheduling state (bitset, counters) and sends RETRIEVE_ITEM(firstItem) to PageLocsRetriever.

## 3. Steady-state background computation

1. PageLocsRetriever receives RETRIEVE_ITEM(itemref).
2. Retriever builds page boundaries for the item and calls pageLocs.insert(id, info) for each page.
3. Retriever sends ITEM_READY(itemref) (or negative encoded itemref on failure).
4. Control marks item done in bitset (if valid and not already marked), increments itemsDoneCount, then picks next work:
   - pending ASAP item first,
   - deferred interrupted item next,
   - otherwise next not-yet-done item in ring order.
5. When all items are done:
   - Control sends COMPLETED to retriever.
   - Control calls pageLocs.computationCompleted().
6. PageLocs::computationCompleted() finalizes page numbering, marks controlTaskReadyToBeStopped=true, and defers .locs save using pendingSave=true.

## 4. Navigation requests and on-demand ASAP retrieval

BookController navigation methods call:
- pageLocs.getNextPageId(...)
- pageLocs.getPrevPageId(...)
- pageLocs.getPageId(...)

Lookup behavior inside PageLocs:
1. PageLocs checks the map under mutex.
2. If target data is missing and computation is not completed, checkAndFind() triggers retrieveAsap(itemref).
3. retrieveAsap(itemref):
   - Sends GET_ASAP(itemref) to PageLocsControl.
   - Waits for manager-queue response (ASAP_READY) with timeout.
4. Control handling for GET_ASAP:
   - If item already done in bitset: immediate ASAP_READY to PageLocs.
   - If another item is currently running: queue GET_ASAP to retriever and set asapItemref.
   - If requested item is currently running: remember asapItemref so control will reply when current ITEM_READY arrives.
   - If idle: dispatch GET_ASAP directly to retriever.
5. Retriever preemption model:
   - At page boundaries, retriever polls pending queue.
   - On GET_ASAP for a different item: it sets abortCurrentItem=true and interrupts the current item safely.
   - Sends ITEM_INTERRUPTED(previousItem) to control.
6. Control on ITEM_INTERRUPTED:
   - Stores interrupted item as deferred nextItemrefToGet.
   - Dispatches ASAP item immediately if pending.
7. Retriever computes ASAP item and sends ASAP_READY(itemref) to control.
8. Control forwards ASAP_READY to PageLocs (mgrQueue).
9. PageLocs wakes, retries map lookup, and returns PageId to BookController.

## 5. Stop, teardown, and persistence

1. On book switch or explicit stop, BookController calls pageLocs.stopControlTask().
2. PageLocs sends STOP to control and waits for STOPPED on mgrQueue.
3. Control requests retriever stop (requestStop + STOP), waits for retriever thread exit, then notifies STOPPED.
4. PageLocs joins control thread and resets controlTask.
5. If pendingSave is true, PageLocs saves .locs after worker teardown.

## 6. Threading and synchronization points

- PageLocs map access is protected by recursive_timed_mutex.
- During retrieveAsap()/getPageCountOrPercent(), a relax reference counter indicates manager waiters; insertion can bypass timed lock contention safely in that phase.
- Control and retriever communicate through dedicated queues, avoiding direct shared-state mutation for scheduling commands.
- Retriever interruption is cooperative at page boundaries, not asynchronous mid-operation.

## 7. Error and defensive paths

- Invalid GET_ASAP indices are rejected by control with ASAP_READY carrying negative/error encoding.
- Retriever failures encode item index as negative in ITEM_READY/ASAP_READY payload.
- If bitset allocation fails at START_DOCUMENT, control aborts computation via pageLocs.computationAborted(...).
- PageLocs retrieveAsap() uses timeout and returns false on no reply, preventing indefinite navigation blocking.

## 8. Practical behavior summary

- BookController stays synchronous and simple: all complexity is behind PageLocs.
- PageLocs provides immediate lookup when possible and bounded wait for missing data.
- PageLocsControl ensures responsive user-facing retrieval by prioritizing GET_ASAP over sequential completion.
- PageLocsRetriever provides the heavy computation and supports safe interruption/restart to maintain UI responsiveness.

## 9. Concurrency risk analysis: pitfalls, deadlocks, synchronization issues

This section highlights potential failure modes observed from the current implementation.

### 9.1 Shared manager queue can misroute replies between waiters

Observed pattern:
- PageLocs::retrieveAsap() and PageLocs::getPageCountOrPercent() both wait on the same mgrQueue.
- Both consumers accept whichever message arrives first and then interpret it according to their local expectation.

Risk:
- If two callers block concurrently, one caller can consume the other's response (ASAP_READY vs PERCENT).
- This can produce timeouts, wrong return values, and unstable navigation/progress behavior.
- A STOPPED message can also be consumed by an unrelated waiter, leaving teardown logic with stale state.

Deadlock/liveness impact:
- High liveness risk due to cross-consumer message stealing on a single FIFO queue.

Suggested mitigations:
- Introduce correlation IDs in queue payloads and discard/requeue non-matching responses.
- Or split mgrQueue into per-purpose channels (ASAP/PERCENT/STOP control notifications).
- Or enforce a single manager-side consumer and deliver results through per-request synchronization objects.

### 9.2 Lock elision in insert() is unsafe when relax is raised by non-locking callers

Observed pattern:
- pageLocs.insert() skips mutex locking when relax > 0.
- Assumption in comment: a blocking waiter holds the PageLocs mutex while waiting on mgrQueue.

Risk:
- getPageCountOrPercent() increments relax but does not hold PageLocs mutex.
- During this window, retriever can mutate pagesMap/itemsSet without lock while other readers/writers may concurrently access under lock.
- This is a data-race/undefined-behavior risk on non-thread-safe containers.

Deadlock/liveness impact:
- Primary risk is memory corruption / inconsistent map state rather than deadlock.

Suggested mitigations:
- Replace relax-based lock elision with a strict single locking policy for pagesMap/itemsSet.
- If contention reduction is needed, isolate queue-wait logic from map lock scope instead of bypassing lock in writer.
- Keep relax only as telemetry/counter, not as a lock ownership proxy.

### 9.3 Unbounded blocking in stopControlTask() waiting for STOPPED

Observed pattern:
- stopControlTask() sends STOP then does blocking receive(queueData) on mgrQueue without timeout.

Risk:
- If STOP message is not processed, dropped, or STOPPED is misrouted/consumed elsewhere, caller can block indefinitely.
- This can freeze book switching or application state transitions.

Deadlock/liveness impact:
- High deadlock/liveness risk because wait has no timeout/fallback.

Suggested mitigations:
- Use timed wait with retry/backoff and explicit error path.
- Validate queueData.req and continue waiting only for STOPPED while safely handling unrelated messages.
- Add watchdog logging including queue depth and control/retriever thread liveness snapshots.

### 9.4 Blocking operations are executed while holding PageLocs mutex

Observed pattern:
- getNextPageId/getPrevPageId/getPageId hold PageLocs mutex and may call retrieveAsap(), which blocks up to timeout.

Risk:
- Long waits under mutex serialize all page map accesses.
- Other operations that need mutex (including map reads for UI flow) may stall behind a single slow ASAP request.

Deadlock/liveness impact:
- Medium liveness risk (priority inversion / responsiveness degradation).

Suggested mitigations:
- Two-phase lookup: check under lock, release lock before wait, reacquire for post-wait lookup.
- Keep lock hold times bounded and independent of queue wait durations.

### 9.5 Queue send return values are mostly ignored

Observed pattern:
- Most PageLocs::send / PageLocsControl::send / PageLocsRetriever::send calls ignore failure return codes.

Risk:
- On queue full/error, control messages may be dropped silently (STOP, GET_ASAP, ITEM_READY, ASAP_READY).
- Silent drops can manifest as apparent deadlocks or random timeouts.

Deadlock/liveness impact:
- Medium-high depending on which message is lost.

Suggested mitigations:
- Check all send results and route failures to explicit recovery paths.
- For critical messages (STOP, STOPPED, ASAP_READY), retry with bounded timeout and telemetry.

### 9.6 Non-atomic state flags accessed from multiple threads

Observed pattern:
- completed, aborted, pendingSave, controlTaskReadyToBeStopped are plain bool and read/written across threads.
- Some accesses are under mutex, others are not.

Risk:
- Data races and stale reads can trigger incorrect branching (e.g., stopping task too early/late, wrong completion status).

Deadlock/liveness impact:
- Medium consistency risk; can amplify other synchronization faults.

Suggested mitigations:
- Protect all accesses with one mutex, or convert cross-thread flags to atomics with explicit memory order.
- Document the ownership/threading model for each state field.

### 9.7 Setup failure path can leave partially initialized pipeline state

Observed pattern:
- PageLocs::setupPagesComputation() calls controlTask->setup(...) but does not check returned bool.

Risk:
- Control/retriever queues or threads may not exist, while caller continues as if pipeline started.
- Follow-up waits may block or report misleading errors.

Deadlock/liveness impact:
- Medium risk due to invalid lifecycle assumptions.

Suggested mitigations:
- Check setup return value and abort/recover immediately on failure.
- Reset controlTask and expose explicit startup status to callers.

### 9.8 Edge case: itemrefCount == 0 in requestNextItem()

Observed pattern:
- requestNextItem() computes modulo itemrefCount.

Risk:
- If itemrefCount is 0 (malformed/empty spine), modulo-by-zero is undefined behavior.
- Could crash control thread and strand manager waits.

Deadlock/liveness impact:
- Medium risk (crash -> apparent hang in waiting threads).

Suggested mitigations:
- Guard START_DOCUMENT for itemrefCount <= 0 and finish early with computationAborted or completed-empty semantics.

## 10. Recommended hardening order

1. Remove shared-response ambiguity: split mgr replies or add correlation IDs.
2. Eliminate lock elision based on relax and enforce strict map locking.
3. Add bounded waits/timeouts and robust message filtering in stopControlTask().
4. Validate and handle all queue send failures.
5. Normalize flag synchronization (mutex or atomics, consistently).
6. Harden startup and empty-document edge paths.

## 11. Stress scenarios to validate fixes

1. Concurrent navigation threads issuing getNext/getPrev while periodic getPageCountOrPercent is active.
2. Rapid book switch loops during active ASAP retrieval (start/stop churn).
3. Artificial queue saturation (small queue depth + delayed consumer).
4. Fault injection for send failures and retriever setup failure.
5. Empty spine / invalid itemref requests and repeated GET_ASAP to same/different items.
