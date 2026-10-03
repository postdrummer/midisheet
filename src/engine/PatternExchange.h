#pragma once

// Lock-free handoff of compiled sheets from the message thread to the audio
// thread. The audio thread never allocates or frees: replaced sheets are
// parked in `retired` and deleted by the message thread on its next publish().

#include "ArpEngine.h"

#include <atomic>
#include <memory>

namespace arp {

template <typename T>
class Exchange {
public:
    Exchange() : live(new T()) {}
    ~Exchange()
    {
        delete live;
        delete pending.load();
        delete retired.load();
    }
    Exchange(const Exchange&) = delete;
    Exchange& operator=(const Exchange&) = delete;

    // Message thread.
    void publish(std::unique_ptr<T> next)
    {
        delete retired.exchange(nullptr, std::memory_order_acq_rel);
        delete pending.exchange(next.release(), std::memory_order_acq_rel); // never seen by audio
    }

    // Audio thread. The reference stays valid until the next call.
    const T& acquire()
    {
        if (pending.load(std::memory_order_acquire) != nullptr
            && retired.load(std::memory_order_acquire) == nullptr) {
            if (T* next = pending.exchange(nullptr, std::memory_order_acq_rel)) {
                retired.store(live, std::memory_order_release);
                live = next;
            }
        }
        return *live;
    }

private:
    T* live;                          // owned by the audio thread
    std::atomic<T*> pending{nullptr};
    std::atomic<T*> retired{nullptr};
};

} // namespace arp
