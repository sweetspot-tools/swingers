#pragma once

#include "Wear.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

namespace swingers
{
// Draws the seeded wear (see Wear.h). Two instances make a view: the Background layer sits behind every
// control (panel colour, fine grain, scratches, stains, screws), the Tape layer sits above them (paper
// strips with handwriting and glue, kept clear of the controls). Neither layer takes mouse clicks.
// The picture is cached in an image, rebuilt only on resize, new seed, new keep-clear list or new pixel scale.
// The soft shadow under a tape piece (black, 50 %, offset 0.8 / 1.6, light blur), drawn before its paper
// (outline in the strip's own coordinates).
void drawTapeShadow (juce::Graphics&, const juce::Path& outline);

class WearLayer : public juce::Component
{
public:
    enum class Layer { Background, Tape };

    explicit WearLayer (Layer layer = Layer::Background);

    void setSeed (uint32_t newSeed);
    uint32_t getSeed() const { return seed; }

    // Rectangles (in this component's coordinates) that tape and stains must not cover.
    void setKeepClear (std::vector<juce::Rectangle<int>> rectangles);
    const std::vector<juce::Rectangle<int>>& getKeepClear() const { return keepClear; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void rebuild (float scale);

    Layer layer;
    uint32_t seed = 0;
    std::vector<juce::Rectangle<int>> keepClear;

    juce::Image cache;
    float cachedScale = 0.0f;
    bool dirty = true;
    juce::Typeface::Ptr tapeTypeface;
};
} // namespace swingers
