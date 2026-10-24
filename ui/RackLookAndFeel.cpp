#include "RackLookAndFeel.h"

namespace swingers
{
namespace
{
juce::String pickTypefaceName()
{
    const auto available = juce::Font::findAllTypefaceNames();
    for (const char* wanted : { "Arial Narrow", "Liberation Sans Narrow", "Helvetica Neue", "Arial",
                                "Liberation Sans", "DejaVu Sans" })
        if (available.contains (wanted, true))
            return wanted;
    return juce::Font::getDefaultSansSerifFontName();
}
} // namespace

RackLookAndFeel::RackLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::panel);

    setColour (juce::ComboBox::backgroundColourId, colours::comboFill);
    setColour (juce::ComboBox::textColourId, colours::white);
    setColour (juce::ComboBox::outlineColourId, colours::comboBorder);
    setColour (juce::ComboBox::arrowColourId, colours::silkscreen);
    setColour (juce::ComboBox::focusedOutlineColourId, colours::amber);

    setColour (juce::PopupMenu::backgroundColourId, colours::comboFill);
    setColour (juce::PopupMenu::textColourId, colours::white);
    setColour (juce::PopupMenu::headerTextColourId, colours::silkscreen);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::amber);
    setColour (juce::PopupMenu::highlightedTextColourId, colours::panel);

    setColour (juce::TextButton::buttonColourId, colours::faceButton);
    setColour (juce::TextButton::buttonOnColourId, colours::faceButton);
    setColour (juce::TextButton::textColourOffId, colours::white);
    setColour (juce::TextButton::textColourOnId, colours::white);

    setColour (juce::ScrollBar::thumbColourId, colours::faceButtonBorder);
    setColour (juce::ScrollBar::trackColourId, colours::comboFill);

    setColour (juce::Label::textColourId, colours::silkscreen);
    setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);

    setColour (juce::TooltipWindow::backgroundColourId, colours::comboFill);
    setColour (juce::TooltipWindow::textColourId, colours::white);
    setColour (juce::TooltipWindow::outlineColourId, colours::comboBorder);
}

juce::Font RackLookAndFeel::silkscreenFont (float size, float extraKerning)
{
    static const auto name = pickTypefaceName();
    // JUCE font height covers ascent + descent: about 1.15 x the em size.
    return juce::Font (juce::FontOptions (name, size * 1.15f, juce::Font::bold)).withExtraKerningFactor (extraKerning);
}

void RackLookAndFeel::drawBoxWithTitle (juce::Graphics& g, juce::Rectangle<int> box, const juce::String& title,
                                        TitleSide side)
{
    const auto font = silkscreenFont (11.0f);
    const auto text = title.toUpperCase();
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (font, text, 0.0f, 0.0f);
    const auto textWidth = glyphs.getBoundingBox (0, -1, true).getWidth();
    const auto gapWidth = (int) std::ceil (textWidth) + 18;

    const auto edgeY = side == TitleSide::top ? box.getY() : box.getBottom();
    const juce::Rectangle<int> gap (box.getCentreX() - gapWidth / 2, edgeY - 3, gapWidth, 6);

    g.saveState();
    g.excludeClipRegion (gap);
    g.setColour (colours::white);
    g.drawRoundedRectangle (box.toFloat(), 9.0f, 1.6f);
    g.restoreState();

    g.setFont (font);
    g.setColour (colours::white);
    g.drawText (text, juce::Rectangle<int> (box.getCentreX() - gapWidth / 2, edgeY - 8, gapWidth, 16),
                juce::Justification::centred, false);
}

void RackLookAndFeel::drawLed (juce::Graphics& g, juce::Point<float> centre, bool lit, juce::Colour litColour)
{
    const auto bounds = juce::Rectangle<float> (7.2f, 7.2f).withCentre (centre);
    g.setColour (lit ? litColour : colours::ledOff);
    g.fillEllipse (bounds);
    g.setColour (juce::Colours::black);
    g.drawEllipse (bounds, 0.6f);
}

void RackLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int,
                                    juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<float> (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (box.findColour (box.hasKeyboardFocus (true) ? juce::ComboBox::focusedOutlineColourId
                                                             : juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

    // Chevron, like the preview: m 0 0 l 5 5 l 5 -5.
    const auto x = (float) width - 16.0f, y = (float) height * 0.5f - 2.5f;
    juce::Path chevron;
    chevron.startNewSubPath (x, y);
    chevron.lineTo (x + 5.0f, y + 5.0f);
    chevron.lineTo (x + 10.0f, y);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId).withAlpha (box.isEnabled() ? 1.0f : 0.4f));
    g.strokePath (chevron, juce::PathStrokeType (1.5f));
}

juce::Font RackLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return silkscreenFont (12.0f);
}

void RackLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (juce::Rectangle<int> (9, 1, box.getWidth() - 9 - 22, box.getHeight() - 2));
    label.setFont (getComboBoxFont (box));
}

void RackLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                            bool highlighted, bool down)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    // A toggle that is on gets the amber face (the preview's lit face button).
    const bool lit = button.getClickingTogglesState() && button.getToggleState();
    auto fill = lit ? colours::amber : colours::faceButton;
    if (highlighted)
        fill = fill.brighter (0.08f);
    if (down)
        fill = fill.darker (0.2f);
    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (lit ? colours::faceButtonLitBorder : colours::faceButtonBorder);
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);
}

juce::Font RackLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return silkscreenFont (11.0f);
}

void RackLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    g.setFont (getTextButtonFont (button, button.getHeight()));
    const bool lit = button.getClickingTogglesState() && button.getToggleState();
    g.setColour ((lit ? colours::faceButtonLitText : button.findColour (juce::TextButton::textColourOffId))
                     .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f));
    g.drawText (button.getButtonText().toUpperCase(), button.getLocalBounds(), juce::Justification::centred, false);
}

juce::Font RackLookAndFeel::getPopupMenuFont()
{
    return silkscreenFont (12.0f);
}

void RackLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
    g.setColour (colours::comboBorder);
    g.drawRect (0, 0, width, height, 1);
}
} // namespace swingers
