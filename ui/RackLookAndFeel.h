#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace swingers
{
namespace colours
{
inline const juce::Colour panel { 0xff161617 };
inline const juce::Colour silkscreen { 0xffc9c9c2 };
inline const juce::Colour white { 0xfff4f4f0 };
inline const juce::Colour amber { 0xfff0b429 };
inline const juce::Colour yellowLed { 0xfff2c21b };
inline const juce::Colour ledOff { 0xff2a2a2c };
inline const juce::Colour comboFill { 0xff0b0b0c };
inline const juce::Colour comboBorder { 0xff3a3a3d };
inline const juce::Colour faceButton { 0xff3a3833 };
inline const juce::Colour faceButtonBorder { 0xff57544b };
inline const juce::Colour cycleFace { 0xffb9ae8d };
inline const juce::Colour cycleBorder { 0xffd8cfb0 };
} // namespace colours

// Flat rack look: dark panel, white-bordered boxes, bold upper-case silkscreen, no gradients, no glow.
class RackLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RackLookAndFeel();

    // Narrow bold sans (Arial Narrow where installed, otherwise the nearest available sans).
    // `size` is the em size in pixels, as the preview's font-size.
    static juce::Font silkscreenFont (float size, float extraKerning = 0.0f);

    enum class TitleSide { top, bottom };
    // White rounded box, the border interrupted behind a title centred on the top or bottom edge.
    static void drawBoxWithTitle (juce::Graphics&, juce::Rectangle<int> box, const juce::String& title,
                                  TitleSide side);

    // Round LED without glow: fill, thin dark outline.
    static void drawLed (juce::Graphics&, juce::Point<float> centre, bool lit, juce::Colour litColour);

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown, int buttonX, int buttonY,
                       int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;

    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
};
} // namespace swingers
