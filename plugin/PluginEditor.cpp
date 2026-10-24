#include "PluginEditor.h"

namespace swingers
{
SwingersEditor::SwingersEditor (SwingersProcessor& p)
    : juce::AudioProcessorEditor (p), processor (p), compact (p)
{
    setLookAndFeel (&lookAndFeel);
    setResizable (false, false);

    addAndMakeVisible (wearBackground);
    addAndMakeVisible (compact);
    addAndMakeVisible (wearTape);
    wearBackground.setComponentID ("wearBackground");
    wearTape.setComponentID ("wearTape");
    wearBackground.setSeed (processor.wearSeed());
    wearTape.setSeed (processor.wearSeed());
    setWearKeepClear (CompactPanel::wearKeepClear());
    compact.setComponentID ("compactPanel");
    compact.onEditMapping = [this] { showPatchbay (true); };

    processor.onStateChanged = [this] {
        compact.refresh();
        wearBackground.setSeed (processor.wearSeed());
        wearTape.setSeed (processor.wearSeed());
    };

    setSize (CompactPanel::kWidth, CompactPanel::kHeight);
}

SwingersEditor::~SwingersEditor()
{
    processor.onStateChanged = nullptr;
    setLookAndFeel (nullptr);
}

void SwingersEditor::showPatchbay (bool show)
{
    patchbayShown = show;
}

void SwingersEditor::setWearKeepClear (const std::vector<juce::Rectangle<int>>& rectangles)
{
    wearBackground.setKeepClear (rectangles);
    wearTape.setKeepClear (rectangles);
}

void SwingersEditor::resized()
{
    wearBackground.setBounds (getLocalBounds());
    wearTape.setBounds (getLocalBounds());
    compact.setBounds (getLocalBounds());
}
} // namespace swingers
