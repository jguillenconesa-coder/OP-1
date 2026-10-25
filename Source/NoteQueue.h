// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <array>
#include <atomic>
#include <cstddef>
namespace pocket {
struct NoteEvent {int channel=1,note=60;float velocity=0;bool on=false;};
// Single producer/single consumer. UI/audio each own a queue in their direction.
static_assert(std::atomic<size_t>::is_always_lock_free,"Realtime note queues require lock-free atomic indices");
class NoteQueue {
public:
    bool push(NoteEvent event) noexcept {
        const auto w=write.load(std::memory_order_relaxed),next=(w+1)%events.size();
        if(next==read.load(std::memory_order_acquire)) return false;
        events[w]=event;write.store(next,std::memory_order_release);return true;
    }
    bool pop(NoteEvent& event) noexcept {
        const auto r=read.load(std::memory_order_relaxed);
        if(r==write.load(std::memory_order_acquire)) return false;
        event=events[r];read.store((r+1)%events.size(),std::memory_order_release);return true;
    }
private:
    std::array<NoteEvent,1024> events{};
    std::atomic<size_t> read{0},write{0};
};
}
