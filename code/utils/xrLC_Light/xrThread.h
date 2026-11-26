#pragma once

#include <atomic>
#include <thread>

class XRLC_LIGHT_API CThread {
    static void startup(void* P);

public:
    volatile u32 thID;
    volatile float thProgress;
    volatile BOOL thCompleted;
    volatile BOOL thMessages;
    volatile BOOL thMonitor;
    volatile float thPerformance;
    volatile BOOL thDestroyOnComplete;

    CThread(u32 _ID) {
        thID = _ID;
        thProgress = 0;
        thCompleted = FALSE;
        thMessages = TRUE;
        thMonitor = FALSE;
        thDestroyOnComplete = TRUE;
    }
    virtual ~CThread() {}
    // The manager waits on thCompleted, not on the thread: detached (a joinable std::thread destroyed unjoined
    // calls std::terminate, which ended every threaded xrLC phase).
    void Start() { std::thread(startup, this).detach(); }
    virtual void Execute() = 0;
};

class XRLC_LIGHT_API CThreadManager {
    xr_vector<CThread*> threads;

public:
    void start(CThread* T);
    void wait(u32 sleep_time = 1000);
};

// Worker threads for the phases that split their work into independent parts: the machine's hardware threads (GSC
// fixed 3 to 8 per phase, for the machines of 2007).
IC u32 lc_worker_threads() {
    const u32 Count = std::thread::hardware_concurrency();
    return Count ? Count : 4;
}

// IX-Ray's way of running a phase (xr_std_parallel_for): Worker(thread_index) on one thread per hardware thread, all
// joined before this returns. The workers take their items from a counter they share (lc_take), so the load balances
// itself, and each worker keeps its own state (collider, hash, selected lights) across its items.
template <typename Worker>
void lc_run_workers(Worker&& worker, u32 threads = lc_worker_threads()) {
    std::vector<std::thread> pool; // (xr_vector's allocator copies elements: threads only move)
    pool.reserve(threads);
    for (u32 index = 0; index < threads; ++index)
        pool.emplace_back([&worker, index]() {
            set_current_thread_name("worker-thread");
            FPU::m64r();
            worker(index);
        });
    for (std::thread& thread : pool)
        thread.join();
}

// The next item for a worker of lc_run_workers: false when all of [0, count) are taken. step > 1 hands out runs of
// items (rows of a lightmap), first is the run's first item.
IC bool lc_take(std::atomic<u32>& next, u32 count, u32& first, u32 step = 1) {
    first = next.fetch_add(step);
    return first < count;
}

IC void get_intervals(u32 max_threads, u32 num_items, u32& threads, u32& stride, u32& rest) {
    if (max_threads <= num_items) {
        threads = max_threads;
        stride = num_items / max_threads;
        rest = num_items % max_threads;
        return;
    }
    threads = num_items;
    stride = 1;
    rest = 0;
}