#pragma once

class ENGINE_API CThread {
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
    // calls std::terminate).
    void Start() { std::thread(startup, this).detach(); }
    virtual void Execute() = 0;
};

class ENGINE_API CThreadManager {
    xr_vector<CThread*> threads;

public:
    void start(CThread* T);
    void wait(u32 sleep_time = 1000);
};