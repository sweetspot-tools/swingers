#pragma once

#include "DrumMap.h"
#include "MapLibrary.h"
#include "PairOverrides.h"

#include <juce_core/juce_core.h>

#include <cstdint>
#include <optional>

namespace swingers
{
// What a project remembers: which maps are chosen, the channel filter, and copies of the maps and
// overrides so the project plays the same on a computer that lacks them.
struct PluginState
{
    juce::String fromId { kGeneralMidiId }, toId { kGeneralMidiId };
    int channel = 0; // 0 = all channels, 1..16
    std::optional<DrumMap> fromCopy, toCopy;
    std::optional<PairOverrides> overridesCopy;
    uint32_t wearSeed = 0; // seed of the panel wear; 0 = not created yet (the processor picks one)
};

// JSON text: {"format":"swingers-state","version":1,"from":..,"to":..,"channel":..,
//             "fromMap":{..},"toMap":{..},"overrides":{..},"wearSeed":..}
juce::MemoryBlock serialize (const PluginState&);

// nullopt when the data is not a version 1 "swingers-state". A missing or broken copy is just nullopt
// for that copy.
std::optional<PluginState> deserialize (const void* data, size_t size);

struct ResolvedMaps
{
    DrumMap from, to;
    PairOverrides overrides;
    juce::StringArray warnings;
};

// The library version of a map wins; otherwise the copy saved in the project (with a warning);
// otherwise General MIDI. Overrides: the library pair file if there is one, otherwise the copy.
ResolvedMaps resolve (const PluginState&, const MapLibrary&);
} // namespace swingers
