#pragma once

#include "DrumMap.h"
#include "PairOverrides.h"

#include <array>
#include <cstdint>

namespace swingers
{
enum class Route : uint8_t { Standard, Override, DroppedByUser, NoMatch };

struct RouteEntry
{
    int target = -1;
    Route route = Route::NoMatch;
    int gm = kNoGm; // GM voice of the source note
};

// Maps each of the 128 source notes to a target note through the General MIDI pivot.
struct ConversionTable
{
    std::array<RouteEntry, 128> entries {};

    // Order per note: direct override, dropped by the user, GM voice of the source note
    // (target: the preferred note of that voice, the lowest preferred if several, else the lowest note),
    // otherwise no match. When from.id == to.id the standard route is the identity (every note to itself),
    // with the pair overrides applied on top.
    static ConversionTable build (const DrumMap& from, const DrumMap& to, const PairOverrides* overrides);

    // Every note to itself, Route::Standard, gm = the note.
    static ConversionTable identity();
};
} // namespace swingers
