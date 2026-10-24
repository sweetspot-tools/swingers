#pragma once

#include "DrumMap.h"

#include <juce_core/juce_core.h>

#include <optional>

namespace swingers
{
// Imports a Cubase drum map XML. Tolerant of `<`, `>` and `&` written unescaped inside attribute values.
// Voices with a missing or out-of-range INote/ONote, and voices named "---" or without a name, are skipped.
// A Cubase ONote of 0 means "no GM voice". Returns nullopt and fills `error` when the file is not a usable map.
std::optional<DrumMap> importCubaseDrumMap (const juce::String& xmlText, juce::String& error);
} // namespace swingers
