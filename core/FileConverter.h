#pragma once

#include "ConversionTable.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <map>

namespace swingers
{
struct ConversionReport
{
    int tracksTouched = 0;
    int notesConverted = 0;
    std::map<int, int> droppedNoMatch, droppedByUser; // source note -> count of note-ons
    juce::String error;

    bool ok() const { return error.isEmpty(); }
};

// Tick-exact: timestamps (ticks), time format and track layout are copied as they are, only note numbers
// change and unroutable notes disappear. channel: 0 = all, 1..16 = only that channel.
// On error `out` is left empty.
ConversionReport convertMidiFile (const juce::MidiFile& in, int fileType, const ConversionTable& table,
                                  int channel, juce::MidiFile& out);

// Reads `in`, writes `out` with the same file type; never overwrites the input and never leaves a half file.
ConversionReport convertMidiFile (const juce::File& in, const juce::File& out, const ConversionTable& table,
                                  int channel);
} // namespace swingers
