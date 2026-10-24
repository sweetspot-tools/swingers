#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace swingers
{
// The panel itself, drawn behind every control: flat #161617 plus the corner screws. Texture, wear and
// tape pieces are added here later; controls never draw them.
class RackBackground : public juce::Component
{
public:
    RackBackground();
    void paint (juce::Graphics&) override;

    // Screw positions of the compact panel (520 x 140), as in the preview.
    static void drawScrew (juce::Graphics&, juce::Point<float> centre, float angleDegrees);
};
} // namespace swingers
