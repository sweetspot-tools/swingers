#pragma once

#include "ConversionTable.h"
#include "DrumMap.h"

#include <juce_core/juce_core.h>

#include <vector>

namespace swingers
{
enum class SourceState { Connected, NoMatch, DroppedByUser };

struct PatchRow
{
    int note = 0;
    juce::String name;
    SourceState state = SourceState::NoMatch;
};

// One cable of the patchbay. `gm` is the GM voice a standard cable goes through (kNoGm for a direct one).
struct PatchCable
{
    int source = 0;
    int gm = kNoGm;
    int target = 0;
    bool direct = false;

    bool operator== (const PatchCable&) const = default;
};

// What the patchbay shows for a (from, to) pair: rows on both sides, the GM voices in the middle and the cables.
struct PatchbayModel
{
    std::vector<PatchRow> sources;  // notes of `from` (all 128 if allNotes), by note
    std::vector<MapNote> targets;   // notes of `to` (all 128 if allNotes), by note
    std::vector<int> gmVoices;      // GM voices used by the standard cables, ascending; empty if !showGmColumn
    std::vector<PatchCable> cables; // one per connected source
    bool showGmColumn = true;       // false when from or to is the GM map

    // Notes missing from a map show up (with an empty name) only when allNotes is set.
    static PatchbayModel build (const DrumMap& from, const DrumMap& to, const ConversionTable&, bool allNotes);

    const PatchRow* findSource (int note) const;
    int targetIndexOf (int note) const; // row in `targets`, -1 if not listed
};
} // namespace swingers
