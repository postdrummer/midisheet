#pragma once

// Lock-free handoff of compiled patterns from the message thread to the audio
// thread. The audio thread never allocates or frees: replaced patterns are
// parked in `retired` and deleted by the message thread on its next publish().

#include "ArpEngine.h"

#include <atomic>
#include <memory>

namespace arp {

class PatternExchange {
public:
    PatternExchange() : live(new Pattern()) {}
    ~PatternExchange()
    {
        delete live;
        delete pending.load();
        delete retired.load();
    }
    PatternExchange(const PatternExchange&) = delete;
    PatternExchange& operator=(const PatternExchange&) = delete;

    // Message thread.
    void publish(std::unique_ptr<Pattern> next)
    {
        delete retired.exchange(nullptr, std::memory_order_acq_rel);
        delete pending.exchange(next.release(), std::memory_order_acq_rel); // never seen by audio
    }

    // Audio thread. The reference stays valid until the next call.
    const Pattern& acquire()
    {
        if (pending.load(std::memory_order_acquire) != nullptr
            && retired.load(std::memory_order_acquire) == nullptr) {
            if (Pattern* next = pending.exchange(nullptr, std::memory_order_acq_rel)) {
                retired.store(live, std::memory_order_release);
                live = next;
            }
        }
        return *live;
    }

private:
    Pattern* live;                        // owned by the audio thread
    std::atomic<Pattern*> pending{nullptr};
    std::atomic<Pattern*> retired{nullptr};
};

} // namespace arp
