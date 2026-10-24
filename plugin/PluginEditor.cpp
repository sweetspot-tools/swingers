#include "PluginEditor.h"

namespace swingers
{
SwingersEditor::SwingersEditor (SwingersProcessor& p)
    : juce::AudioProcessorEditor (p), processor (p), compact (p)
{
    setLookAndFeel (&lookAndFeel);
    setResizable (false, false);

    addAndMakeVisible (compact);
    compact.setComponentID ("compactPanel");
    compact.onEditMapping = [this] { showPatchbay (true); };

    processor.onStateChanged = [this] { compact.refresh(); };

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

void SwingersEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
}

void SwingersEditor::resized()
{
    compact.setBounds (getLocalBounds());
}
} // namespace swingers
