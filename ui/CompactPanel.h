#pragma once

#include "MapListBox.h"
#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace swingers
{
// Small amber LED: lit or not, nothing else (no glow).
class ActivityLed : public juce::Component
{
public:
    void setLit (bool shouldBeLit);
    bool isLit() const { return lit; }
    void paint (juce::Graphics&) override;

private:
    bool lit = false;
};

// Cycling selector for the MIDI channel: CH label, a face button, a yellow LED (lit when not ALL) and the
// value under it (ALL, 1..16). A click moves to the next value and wraps around.
class ChannelCycle : public juce::Component, public juce::SettableTooltipClient
{
public:
    static constexpr int kValues = 17; // ALL + 16 channels

    int getIndex() const { return index; } // 0 = ALL, 1..16
    void setIndex (int newIndex);          // no callback
    void cycle();                          // next value, with callback
    juce::String getValueText() const;
    bool isLedLit() const { return index != 0; }

    std::function<void (int)> onChange;

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    int index = 0;
};

// Boxes, titles and the arrow of the compact panel: a layer between the background and the controls.
class CompactSilkscreen : public juce::Component
{
public:
    CompactSilkscreen() { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override;
};

// The panel shown when the plugin opens: From -> To, channel, MIDI activity LED, "Edit mapping".
class CompactPanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int kWidth = 520;
    static constexpr int kHeight = 140;

    explicit CompactPanel (SwingersProcessor&);
    ~CompactPanel() override;

    std::function<void()> onEditMapping;

    // Re-reads maps, selection, channel and warnings from the processor (no change notifications).
    void refresh();

    // The 30 Hz timer calls this with the millisecond counter: the LED stays lit for 80 ms after
    // remapper().activity() changes. Public so tests can drive time.
    void pollActivity (uint32_t nowMs);
    bool isLedLit() const { return led.isLit(); }

    // Where the wear must stay clear: the controls, the printed text and the warnings row (panel coordinates).
    static std::vector<juce::Rectangle<int>> wearKeepClear();

    void resized() override;

private:
    void timerCallback() override;
    void fillMapBox (juce::ComboBox&) const;
    void selectCurrentMaps();
    void updateWarnings();

    SwingersProcessor& processor;
    CompactSilkscreen silkscreen;
    MapListBox fromBox, toBox;
    ChannelCycle channelCycle;
    juce::TextButton editMappingButton { "Edit mapping" };
    ActivityLed led;
    juce::Label warningsLabel;

    juce::String listSignature;
    uint32_t seenActivity = 0;
    uint32_t lastHitMs = 0;
    bool hasHit = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompactPanel)
};
} // namespace swingers
