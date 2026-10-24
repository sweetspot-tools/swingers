#include "CompactPanel.h"

#include "RackLookAndFeel.h"

namespace swingers
{
namespace
{
constexpr int kLitMs = 80;
constexpr int kIssueIdBase = 1000; // item ids of the broken files, after the map ids 1..N

// Geometry from the approved preview (520 x 140).
juce::Rectangle<int> fromBoxArea() { return { 150, 14, 140, 46 }; }
juce::Rectangle<int> toBoxArea()   { return { 312, 14, 140, 46 }; }
juce::Rectangle<int> comboIn (juce::Rectangle<int> box) { return { box.getX() + 8, box.getY() + 8, 124, 24 }; }
juce::Rectangle<int> channelArea() { return { 458, 10, 48, 66 }; }
juce::Rectangle<int> buttonArea()  { return { 150, 78, 302, 26 }; }
juce::Rectangle<int> midiLedArea() { return juce::Rectangle<int> (10, 10).withCentre ({ 36, 82 }); }
juce::Rectangle<int> warningsArea() { return { 12, 113, CompactPanel::kWidth - 24, 16 }; }

bool isMapFile (const juce::String& fileName) { return fileName.endsWithIgnoreCase (".swmap.json"); }
} // namespace

void ActivityLed::setLit (bool shouldBeLit)
{
    if (lit == shouldBeLit)
        return;
    lit = shouldBeLit;
    repaint();
}

void ActivityLed::paint (juce::Graphics& g)
{
    RackLookAndFeel::drawLed (g, getLocalBounds().toFloat().getCentre(), lit, colours::amber);
}

void ChannelCycle::setIndex (int newIndex)
{
    newIndex = juce::jlimit (0, kValues - 1, newIndex);
    if (index == newIndex)
        return;
    index = newIndex;
    repaint();
}

void ChannelCycle::cycle()
{
    setIndex ((index + 1) % kValues);
    if (onChange)
        onChange (index);
}

juce::String ChannelCycle::getValueText() const
{
    return index == 0 ? juce::String ("ALL") : juce::String (index);
}

void ChannelCycle::paint (juce::Graphics& g)
{
    // Local origin = the area's top-left; the preview centre line is x = 482 -> 24 here.
    const float cx = (float) getWidth() * 0.5f;

    g.setColour (colours::white);
    g.setFont (RackLookAndFeel::silkscreenFont (10.0f));
    g.drawText ("CH", juce::Rectangle<int> (0, 0, getWidth(), 14), juce::Justification::centred, false);

    const auto face = juce::Rectangle<float> (24.0f, 18.0f).withCentre ({ cx, 29.0f });
    g.setColour (colours::cycleFace);
    g.fillRoundedRectangle (face, 2.0f);
    g.setColour (colours::cycleBorder);
    g.drawRoundedRectangle (face.reduced (0.5f), 2.0f, 1.0f);

    RackLookAndFeel::drawLed (g, { cx, 46.0f }, isLedLit(), colours::yellowLed);

    g.setColour (colours::silkscreen);
    g.setFont (RackLookAndFeel::silkscreenFont (10.0f));
    g.drawText (getValueText(), juce::Rectangle<int> (0, 51, getWidth(), 14), juce::Justification::centred, false);
}

void ChannelCycle::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasClicked() && getLocalBounds().contains (e.getPosition()))
        cycle();
}

CompactPanel::CompactPanel (SwingersProcessor& p) : processor (p)
{
    fromBox.setComponentID ("fromBox");
    toBox.setComponentID ("toBox");
    channelCycle.setComponentID ("channelBox");
    editMappingButton.setComponentID ("editMapping");
    led.setComponentID ("led");
    warningsLabel.setComponentID ("warnings");

    channelCycle.setTooltip ("MIDI channel to convert (click to change): ALL or 1-16");

    fromBox.onChange = [this] {
        const auto index = fromBox.getSelectedId() - 1;
        const auto& maps = processor.library().maps();
        if (index >= 0 && index < (int) maps.size())
            processor.selectMaps (maps[(size_t) index].id, processor.current().to.id);
    };
    toBox.onChange = [this] {
        const auto index = toBox.getSelectedId() - 1;
        const auto& maps = processor.library().maps();
        if (index >= 0 && index < (int) maps.size())
            processor.selectMaps (processor.current().from.id, maps[(size_t) index].id);
    };
    channelCycle.onChange = [this] (int channel) { processor.setChannel (channel); };

    editMappingButton.setTooltip ("Edit the exceptions of this pair");
    editMappingButton.onClick = [this] {
        if (onEditMapping)
            onEditMapping();
    };

    warningsLabel.setColour (juce::Label::textColourId, colours::amber);
    warningsLabel.setFont (RackLookAndFeel::silkscreenFont (10.0f));
    warningsLabel.setJustificationType (juce::Justification::centred);
    warningsLabel.setInterceptsMouseClicks (false, false);

    addAndMakeVisible (background);
    addAndMakeVisible (silkscreen);
    addAndMakeVisible (fromBox);
    addAndMakeVisible (toBox);
    addAndMakeVisible (channelCycle);
    addAndMakeVisible (editMappingButton);
    addAndMakeVisible (led);
    addAndMakeVisible (warningsLabel);

    led.setInterceptsMouseClicks (false, false);
    setSize (kWidth, kHeight);

    seenActivity = processor.remapper().activity();
    refresh();
    startTimerHz (30);
}

CompactPanel::~CompactPanel()
{
    stopTimer();
}

void CompactPanel::fillMapBox (juce::ComboBox& box) const
{
    box.clear (juce::dontSendNotification);

    const auto& maps = processor.library().maps();
    for (size_t i = 0; i < maps.size(); ++i)
        box.addItem (maps[i].name.isNotEmpty() ? maps[i].name : maps[i].id, (int) i + 1);

    const auto& issues = processor.library().issues();
    bool separated = false;
    for (size_t i = 0; i < issues.size(); ++i)
    {
        if (! isMapFile (issues[i].file))
            continue;
        if (! separated)
        {
            box.addSeparator();
            separated = true;
        }
        const auto id = kIssueIdBase + (int) i;
        box.addItem (issues[i].file, id);
        box.setItemEnabled (id, false);
    }
}

void CompactPanel::selectCurrentMaps()
{
    // A map that is only a copy saved in the project is not in the lists: show its name anyway.
    const auto select = [this] (juce::ComboBox& box, const DrumMap& map) {
        const auto& maps = processor.library().maps();
        for (size_t i = 0; i < maps.size(); ++i)
        {
            if (maps[i].id == map.id)
            {
                box.setSelectedId ((int) i + 1, juce::dontSendNotification);
                return;
            }
        }
        box.setText (map.name.isNotEmpty() ? map.name : map.id, juce::dontSendNotification);
    };
    select (fromBox, processor.current().from);
    select (toBox, processor.current().to);
}

void CompactPanel::updateWarnings()
{
    const auto text = processor.current().warnings.joinIntoString ("  |  ");
    warningsLabel.setText (text, juce::dontSendNotification);
    warningsLabel.setTooltip (processor.current().warnings.joinIntoString ("\n"));
}

void CompactPanel::refresh()
{
    // Repopulate the lists only when they changed (import, reload, bad file fixed).
    juce::String signature;
    for (const auto& m : processor.library().maps())
        signature << m.id << '\x1f' << m.name << '\n';
    signature << '\x1e';
    for (const auto& issue : processor.library().issues())
        signature << issue.file << '\x1f' << issue.message << '\n';

    if (signature != listSignature)
    {
        listSignature = signature;
        fillMapBox (fromBox);
        fillMapBox (toBox);

        juce::String tip;
        for (const auto& issue : processor.library().issues())
            tip << issue.file << ": " << issue.message << '\n';
        tip = tip.trimEnd();
        fromBox.setTooltip (tip.isNotEmpty() ? "Files that could not be loaded:\n" + tip : "The drum map your notes are written for");
        toBox.setTooltip (tip.isNotEmpty() ? "Files that could not be loaded:\n" + tip : "The drum map to convert to");
    }

    selectCurrentMaps();
    channelCycle.setIndex (processor.channel());
    updateWarnings();
}

void CompactPanel::pollActivity (uint32_t nowMs)
{
    const auto activity = processor.remapper().activity();
    if (activity != seenActivity)
    {
        seenActivity = activity;
        lastHitMs = nowMs;
        hasHit = true;
    }
    led.setLit (hasHit && (uint32_t) (nowMs - lastHitMs) < (uint32_t) kLitMs);
}

void CompactPanel::timerCallback()
{
    pollActivity (juce::Time::getMillisecondCounter());
}

void CompactSilkscreen::paint (juce::Graphics& g)
{
    using Side = RackLookAndFeel::TitleSide;
    RackLookAndFeel::drawBoxWithTitle (g, fromBoxArea(), "From", Side::bottom);
    RackLookAndFeel::drawBoxWithTitle (g, toBoxArea(), "To", Side::bottom);

    // Arrow between the boxes (drawn as a path: no dependence on the font having the glyph).
    g.setColour (colours::white);
    const juce::Line<float> arrow (293.0f, 37.0f, 309.0f, 37.0f);
    g.drawArrow (arrow, 2.0f, 9.0f, 8.0f);

    g.setColour (colours::white);
    g.setFont (RackLookAndFeel::silkscreenFont (20.0f));
    g.drawText ("SWINGERS", juce::Rectangle<int> (26, 10, 120, 24), juce::Justification::centredLeft, false);
    g.setColour (colours::silkscreen);
    g.setFont (RackLookAndFeel::silkscreenFont (9.0f));
    g.drawText ("DRUM MAP CONVERTER", juce::Rectangle<int> (26, 36, 120, 11), juce::Justification::centredLeft, false);

    g.setColour (colours::white);
    g.setFont (RackLookAndFeel::silkscreenFont (10.0f));
    g.drawText ("MIDI", juce::Rectangle<int> (16, 60, 40, 14), juce::Justification::centred, false);
}

void CompactPanel::resized()
{
    background.setBounds (getLocalBounds());
    silkscreen.setBounds (getLocalBounds());
    fromBox.setBounds (comboIn (fromBoxArea()));
    toBox.setBounds (comboIn (toBoxArea()));
    channelCycle.setBounds (channelArea());
    editMappingButton.setBounds (buttonArea());
    led.setBounds (midiLedArea());
    warningsLabel.setBounds (warningsArea());
}
} // namespace swingers
