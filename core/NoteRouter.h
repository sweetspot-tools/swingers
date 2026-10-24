#pragma once

#include "ConversionTable.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cstdint>

namespace swingers
{
// Routes one MIDI message at a time through a ConversionTable and remembers where every held note went,
// so a note-off always follows its note-on even if the table changes in between.
// Shared by the plugin (LiveRemapper) and the file converter. Not thread safe; no allocation.
class NoteRouter
{
public:
    enum class Outcome { Pass, Remapped, Dropped };

    NoteRouter() { reset(); }

    // channel: 0 = all channels, 1..16 = only that channel is routed.
    // Pass leaves the message untouched; Remapped changes only the note number; Dropped means "do not emit".
    Outcome route (juce::MidiMessage& m, const ConversionTable& table, int channel);

    // Bypass ("pass" mode): a note-on goes out unchanged and is remembered as held on itself; a note-off or
    // aftertouch of a held note follows where its note-on went (so a note started before the bypass gets its
    // off on the remapped target, and one started during the bypass keeps its number after it). Everything
    // else passes untouched.
    Outcome pass (juce::MidiMessage& m);

    // Forgets every held note.
    void reset();

private:
    Outcome followHeld (juce::MidiMessage& m, int16_t& held, int note, bool isNoteOff);

    static constexpr int16_t kNone = -1;
    static constexpr int16_t kDropped = -2;

    // [midi channel 0..15][source note 0..127]: kNone, kDropped, or the target note.
    std::array<int16_t, 16 * 128> held_ {};
};
} // namespace swingers
