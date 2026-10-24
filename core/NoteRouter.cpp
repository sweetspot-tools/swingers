#include "NoteRouter.h"

#include <algorithm>

namespace swingers
{
namespace
{
// A table entry is routable only with a valid target and a Standard/Override route.
// Overrides are not range-checked in memory, so this is deliberately defensive.
int routedTarget (const ConversionTable& table, int note)
{
    const auto& entry = table.entries[(size_t) note];
    const bool routable = entry.route == Route::Standard || entry.route == Route::Override;

    return routable && entry.target >= 0 && entry.target <= 127 ? entry.target : -1;
}
} // namespace

void NoteRouter::reset()
{
    held_.fill (kNone);
}

NoteRouter::Outcome NoteRouter::route (juce::MidiMessage& m, const ConversionTable& table, int channel)
{
    const int size = m.getRawDataSize();
    const auto* raw = m.getRawData();

    if (size != 3 || raw == nullptr)
        return Outcome::Pass;

    const int kind = raw[0] & 0xF0;

    if (kind != 0x80 && kind != 0x90 && kind != 0xA0)
        return Outcome::Pass;

    const int midiChannel = raw[0] & 0x0F;     // 0..15
    const int note = raw[1] & 0x7F;
    const bool isNoteOn = kind == 0x90 && raw[2] > 0;
    const bool isNoteOff = kind == 0x80 || (kind == 0x90 && raw[2] == 0);
    auto& held = held_[(size_t) (midiChannel * 128 + note)];

    const bool excluded = channel >= 1 && channel <= 16 && midiChannel != channel - 1;

    // A note-off or aftertouch for a note we are holding follows it even if the channel filter
    // changed meanwhile, so nothing is left hanging.
    if (excluded && (isNoteOn || held == kNone))
        return Outcome::Pass;

    int target;

    if (isNoteOn)
    {
        // Overwrites a previous entry for the same (channel, note): MIDI has no note stack.
        target = routedTarget (table, note);
        held = (int16_t) (target >= 0 ? target : kDropped);
    }
    else if (held != kNone)
    {
        target = held == kDropped ? -1 : held;

        if (isNoteOff)
            held = kNone;
    }
    else
    {
        target = routedTarget (table, note);
    }

    if (target < 0)
        return Outcome::Dropped;

    m.setNoteNumber (target);
    return Outcome::Remapped;
}
} // namespace swingers
