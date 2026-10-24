#include "RackBackground.h"

#include "RackLookAndFeel.h"

namespace swingers
{
RackBackground::RackBackground()
{
    setInterceptsMouseClicks (false, false);
    setOpaque (true);
}

void RackBackground::drawScrew (juce::Graphics& g, juce::Point<float> centre, float angleDegrees)
{
    g.setColour (juce::Colour (0xff2b2b2e));
    g.fillEllipse (juce::Rectangle<float> (11.2f, 11.2f).withCentre (centre));
    g.setColour (juce::Colour (0xff4c4c50));
    g.drawEllipse (juce::Rectangle<float> (11.2f, 11.2f).withCentre (centre), 1.0f);

    g.saveState();
    g.addTransform (juce::AffineTransform::rotation (juce::degreesToRadians (angleDegrees), centre.x, centre.y));
    g.setColour (juce::Colour (0xff050505));
    g.fillRoundedRectangle (juce::Rectangle<float> (8.0f, 1.8f).withCentre (centre), 0.5f);
    g.fillRoundedRectangle (juce::Rectangle<float> (1.8f, 8.0f).withCentre (centre), 0.5f);
    g.restoreState();
}

void RackBackground::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
    drawScrew (g, { 12.0f, 12.0f }, 20.0f);
    drawScrew (g, { (float) getWidth() - 12.0f, 12.0f }, 105.0f);
}
} // namespace swingers
