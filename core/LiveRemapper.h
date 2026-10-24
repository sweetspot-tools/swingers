#pragma once

#include "NoteRouter.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>
#include <cstdint>

namespace swingers
{
// Realtime MIDI remapper. setTable/setChannel come from the message thread, process from the audio thread.
class LiveRemapper
{
public:
    LiveRemapper();

    // Message thread. The audio thread picks the new table up at the next block where the lock is free.
    void setTable (const ConversionTable& table);

    // 0 = all channels, 1..16 = only that channel.
    void setChannel (int channel);

    // Audio thread. Rewrites the buffer in place, removing Dropped notes. Does not allocate or block
    // (the only exception: a block bigger than the preallocated scratch grows it once, see the .cpp).
    void process (juce::MidiBuffer& buffer);

    // Routed note-ons so far (Remapped only: not Dropped, not passed through).
    uint32_t activity() const noexcept;

    // Routed note-ons for that source note; 0 outside 0..127.
    uint32_t hits (int sourceNote) const noexcept;

private:
    juce::SpinLock pendingLock_;
    ConversionTable pending_;          // guarded by pendingLock_
    bool hasPending_ = false;          // guarded by pendingLock_

    ConversionTable active_;           // audio thread only
    NoteRouter router_;                // audio thread only
    juce::MidiBuffer scratch_;         // audio thread only, preallocated

    std::atomic<int> channel_ { 0 };
    std::atomic<uint32_t> activity_ { 0 };
    std::array<std::atomic<uint32_t>, 128> hits_ {};
};
} // namespace swingers
