#pragma once

#include <juce_core/juce_core.h>

#include <optional>
#include <vector>

namespace swingers
{
inline constexpr int kNoGm = -1;
inline const juce::String kGeneralMidiId { "general-midi" };

struct MapNote
{
    int note = 0;
    juce::String name;
    int gm = kNoGm;
    bool preferred = false;
};

struct DrumMap
{
    juce::String id, name, library;
    std::vector<MapNote> notes; // sorted by note, unique notes 0..127

    bool isGeneralMidi() const;
    const MapNote* find (int note) const;
    int gmVoiceOf (int note) const; // kNoGm if the note is missing or has no voice
    void setPreferred (int note);   // clears `preferred` on the other notes with the same gm voice
};

juce::var toJson (const DrumMap&);

// Returns nullopt when the document is not a version 1 "swingers-map" or lacks an id or a notes array.
// Bad entries (out of range, duplicate, malformed) are skipped and described in `problems`.
std::optional<DrumMap> drumMapFromJson (const juce::var&, juce::StringArray& problems);
} // namespace swingers
