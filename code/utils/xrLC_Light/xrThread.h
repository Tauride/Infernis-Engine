#pragma once

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