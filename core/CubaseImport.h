#pragma once

#include "DrumMap.h"

#include <juce_core/juce_core.h>

#include <optional>

namespace swingers
{
// Imports a Cubase drum map XML. Tolerant of `<`, `>` and `&` written unescaped inside attribute values.
// Voices with a missing or out-of-range INote/ONote, and voices named "---" or without a name, are skipped.
// A Cubase ONote of 0 means "no GM voice". Returns nullopt and fills `error` when the file is not a usable map.
// Lower-case slug of a name for ids: runs of anything but a-z / 0-9 become one '-', no '-' at the ends.
// Empty when the name has no letter or digit.
juce::String slugOf (const juce::String& name);

std::optional<DrumMap> importCubaseDrumMap (const juce::String& xmlText, juce::String& error);
} // namespace swingers
