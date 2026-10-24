#pragma once

#include <juce_graphics/juce_graphics.h>

#include <cstdint>
#include <vector>

namespace swingers
{
// Seeded wear of the panel, as on the Fairy Tapes plugins. This is only the layout (what goes where):
// same seed, same area, same keep-clear rectangles give the same items. Drawing is WearLayer's job.
enum class WearKind { Scratch, Stain, Tape, Screw };

struct WearItem
{
    WearKind kind = WearKind::Scratch;
    // Scratch: start point (x, y), length (w), h = 0. Stain: the box the blob fits in. Tape: the unrotated
    // strip, rotated by `angle` around its centre. Screw: 11.2 px square centred on the screw.
    juce::Rectangle<float> bounds;
    float angle = 0.0f; // degrees; Scratch: direction, Tape: tilt, Screw: resting angle
    juce::String text;  // Tape: the handwriting
    // Sub-seed for the shape details (Scratch, Stain, Tape); Screw: 0 slot, 1 cross, 2 missing (only the hole).
    int variant = 0;

    bool operator== (const WearItem&) const = default;
};

// Wear for a panel of `area` size. Tape pieces and stains never intersect `keepClear` (controls, silkscreen
// text, cables); scratches and screws do not care. Four screws, one per corner.
std::vector<WearItem> generateWear (uint32_t seed, juce::Rectangle<int> area,
                                    const std::vector<juce::Rectangle<int>>& keepClear);

// Axis-aligned box that contains everything an item draws (rotation, torn edges, glue residue, shadow).
juce::Rectangle<float> wearExtent (const WearItem&);

// Handwriting texts a tape piece can carry.
const std::vector<juce::String>& wearTapeTexts();
} // namespace swingers
