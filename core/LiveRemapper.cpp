#include "LiveRemapper.h"

#include <algorithm>

namespace swingers
{
namespace
{
constexpr size_t kScratchBytes = 2048;
// Per-event storage overhead in a juce::MidiBuffer: int32 sample position + uint16 length.
constexpr int kEventOverhead = (int) (sizeof (int32_t) + sizeof (uint16_t));
} // namespace

LiveRemapper::LiveRemapper()
    : pending_ (ConversionTable::identity()),
      active_ (ConversionTable::identity())
{
    scratch_.ensureSize (kScratchBytes);

    for (auto& h : hits_)
        h.store (0, std::memory_order_relaxed);
}

void LiveRemapper::setTable (const ConversionTable& table)
{
    const juce::SpinLock::ScopedLockType lock (pendingLock_);
    pending_ = table;
    hasPending_ = true;
}

void LiveRemapper::setChannel (int channel)
{
    channel_.store (std::clamp (channel, 0, 16), std::memory_order_relaxed);
}

uint32_t LiveRemapper::activity() const noexcept
{
    return activity_.load (std::memory_order_relaxed);
}

uint32_t LiveRemapper::hits (int sourceNote) const noexcept
{
    if (sourceNote < 0 || sourceNote > 127)
        return 0;

    return hits_[(size_t) sourceNote].load (std::memory_order_relaxed);
}

void LiveRemapper::process (juce::MidiBuffer& buffer)
{
    {
        // Never block the audio thread: if the message thread holds the lock, keep the last table.
        const juce::SpinLock::ScopedTryLockType lock (pendingLock_);

        if (lock.isLocked() && hasPending_)
        {
            active_ = pending_;
            hasPending_ = false;
        }
    }

    if (buffer.isEmpty())
        return;

    const int channel = channel_.load (std::memory_order_relaxed);

    // Routed events are never bigger than the input, so the scratch only needs the input's byte count.
    // 2048 bytes are preallocated; ensureSize allocates only if one block exceeds that (grows once, then stays).
    int neededBytes = 0;

    for (const auto meta : buffer)
        neededBytes += meta.numBytes + kEventOverhead;

    scratch_.clear();
    scratch_.ensureSize ((size_t) std::max ((int) kScratchBytes, neededBytes));

    for (const auto meta : buffer)
    {
        // Only 3-byte channel messages are built as MidiMessage (stored inline, no heap);
        // sysex and everything else is copied as raw bytes, never through a MidiMessage.
        const auto* data = meta.data;
        const int kind = data[0] & 0xF0;

        if (meta.numBytes == 3 && (kind == 0x80 || kind == 0x90 || kind == 0xA0))
        {
            juce::MidiMessage message (data, 3, 0.0);
            const int sourceNote = data[1] & 0x7F;
            const bool noteOn = kind == 0x90 && data[2] > 0;

            const auto outcome = router_.route (message, active_, channel);

            if (outcome == NoteRouter::Outcome::Dropped)
                continue;

            if (outcome == NoteRouter::Outcome::Remapped && noteOn)
            {
                activity_.fetch_add (1, std::memory_order_relaxed);
                hits_[(size_t) sourceNote].fetch_add (1, std::memory_order_relaxed);
            }

            scratch_.addEvent (message.getRawData(), message.getRawDataSize(), meta.samplePosition);
            continue;
        }

        scratch_.addEvent (meta.data, meta.numBytes, meta.samplePosition);
    }

    // Copy back instead of swapping: the host buffer keeps its storage and our scratch keeps its capacity.
    // The input already held at least as many bytes, so this does not reallocate.
    buffer.clear();
    buffer.addEvents (scratch_, 0, -1, 0);
}
} // namespace swingers
