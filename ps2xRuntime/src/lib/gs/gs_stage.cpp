#include "runtime/gs/gs_stage.h"
#include "ThreadNaming.h"

#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

namespace
{
    struct Packet
    {
        GifPathId path = GifPathId::Path1;
        std::vector<uint8_t> data;
    };

    // Bound on queued bytes: enough for several XGKICK packets of a batch, small enough that the worker never
    // runs a whole frame ahead of the GS frontend.
    constexpr size_t kMaxQueuedBytes = 4u << 20;
    // A sleeping stage is woken once this much is queued, or by flush() / waitIdle(): one wake for a run of XGKICK packets
    // instead of one per packet (each wake is a system call on the worker, and the stage spins before it sleeps again).
    constexpr size_t kWakeBytes = 64u << 10;

    struct Stage
    {
        std::mutex mutex;
        std::condition_variable work;   // stage waits for packets
        std::condition_variable space;  // producer waits for room
        std::condition_variable idle;   // waitIdle
        std::vector<Packet> queue; // a vector keeps its capacity across swaps (MSVC deque allocates per Packet)
        std::vector<std::vector<uint8_t>> spare; // recycled buffers (avoid an allocation per packet)
        size_t queuedBytes = 0;
        bool busy = false;
        bool waiting = false; // the stage sleeps on `work`
        bool stop = false;
        std::thread thread;
        std::thread::id threadId;
        ps2_gs_stage::ProcessFn process;
        std::function<void()> afterBatch;
    };

    Stage &stage()
    {
        static Stage s;
        return s;
    }

    void loop()
    {
        ThreadNaming::SetCurrentThreadName("GSStage");
        Stage &s = stage();
        std::vector<Packet> batch;
        for (;;)
        {
            {
                std::unique_lock<std::mutex> lock(s.mutex);
                for (Packet &p : batch)
                {
                    p.data.clear();
                    s.spare.push_back(std::move(p.data));
                }
                batch.clear();
                s.busy = false;
                if (s.queue.empty())
                    s.idle.notify_all();
                s.waiting = true;
                s.work.wait(lock, [&s] { return s.stop || !s.queue.empty(); });
                s.waiting = false;
                if (s.queue.empty())
                    return;
                batch.swap(s.queue);
                s.queuedBytes = 0;
                s.busy = true;
            }
            s.space.notify_all();
            for (const Packet &p : batch)
                s.process(p.path, p.data.data(), static_cast<uint32_t>(p.data.size()));
            if (s.afterBatch)
                s.afterBatch();
        }
    }
}

namespace ps2_gs_stage
{
    void start(ProcessFn process, std::function<void()> afterBatch)
    {
        Stage &s = stage();
        std::lock_guard<std::mutex> lock(s.mutex);
        if (s.thread.joinable())
            return;
        s.process = std::move(process);
        s.afterBatch = std::move(afterBatch);
        s.stop = false;
        s.thread = std::thread(loop);
        s.threadId = s.thread.get_id();
        std::fprintf(stderr, "[gs-stage] GS frontend on its own thread, pipelined behind VU1 (PS2X_GS_PIPELINE=0 disables)\n");
    }

    void stop()
    {
        Stage &s = stage();
        {
            std::lock_guard<std::mutex> lock(s.mutex);
            if (!s.thread.joinable())
                return;
            s.stop = true;
        }
        s.work.notify_all();
        s.thread.join();
        s.threadId = {};
    }

    bool running()
    {
        return stage().thread.joinable();
    }

    bool onStageThread()
    {
        return running() && std::this_thread::get_id() == stage().threadId;
    }

    void push(GifPathId path, const uint8_t *data, uint32_t size)
    {
        Stage &s = stage();
        std::unique_lock<std::mutex> lock(s.mutex);
        s.space.wait(lock, [&s, size] { return s.queuedBytes == 0 || s.queuedBytes + size <= kMaxQueuedBytes; });
        Packet p;
        p.path = path;
        if (!s.spare.empty())
        {
            p.data = std::move(s.spare.back());
            s.spare.pop_back();
        }
        p.data.assign(data, data + size);
        s.queuedBytes += size;
        s.queue.push_back(std::move(p));
        const bool wake = s.waiting && s.queuedBytes >= kWakeBytes;
        lock.unlock();
        if (wake)
            s.work.notify_one();
    }

    void flush()
    {
        Stage &s = stage();
        std::unique_lock<std::mutex> lock(s.mutex);
        const bool wake = s.waiting && !s.queue.empty();
        lock.unlock();
        if (wake)
            s.work.notify_one();
    }

    void waitIdle()
    {
        if (!running() || onStageThread())
            return;
        Stage &s = stage();
        std::unique_lock<std::mutex> lock(s.mutex);
        if (s.waiting && !s.queue.empty())
            s.work.notify_one();
        s.idle.wait(lock, [&s] { return s.queue.empty() && !s.busy; });
    }
}
