#include "PluginEditor.h"

namespace swingers
{
SwingersEditor::SwingersEditor (SwingersProcessor& p)
    : juce::AudioProcessorEditor (p), processor (p), compact (p), patchbay (p)
{
    setLookAndFeel (&lookAndFeel);
    setResizable (false, false);

    addAndMakeVisible (wearBackground);
    addAndMakeVisible (compact);
    addChildComponent (patchbay);
    addAndMakeVisible (wearTape);
    wearBackground.setComponentID ("wearBackground");
    wearTape.setComponentID ("wearTape");
    wearBackground.setSeed (processor.wearSeed());
    wearTape.setSeed (processor.wearSeed());
    setWearKeepClear (CompactPanel::wearKeepClear());
    compact.setComponentID ("compactPanel");
    patchbay.setComponentID ("patchbayView");
    compact.onEditMapping = [this] { showPatchbay (true); };
    patchbay.onDone = [this] { showPatchbay (false); };
    patchbay.onLayoutChanged = [this] {
        if (patchbayShown)
            setWearKeepClear (patchbay.wearKeepClear());
    };

    processor.onStateChanged = [this] {
        compact.refresh();
        if (patchbayShown)
            patchbay.refresh();
        wearBackground.setSeed (processor.wearSeed());
        wearTape.setSeed (processor.wearSeed());
    };

    setSize (CompactPanel::kWidth, CompactPanel::kHeight);
}

SwingersEditor::~SwingersEditor()
{
    processor.onStateChanged = nullptr;
    patchbay.onLayoutChanged = nullptr;
    setLookAndFeel (nullptr);
}

void SwingersEditor::showPatchbay (bool show)
{
    patchbayShown = show;
    compact.setVisible (! show);
    patchbay.setVisible (show);
    if (show)
    {
        patchbay.refresh();
        setWearKeepClear (patchbay.wearKeepClear());
        setSize (PatchbayView::kWidth, PatchbayView::kHeight);
    }
    else
    {
        setWearKeepClear (CompactPanel::wearKeepClear());
        setSize (CompactPanel::kWidth, CompactPanel::kHeight);
    }
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
    compact.setBounds (0, 0, CompactPanel::kWidth, CompactPanel::kHeight);
    patchbay.setBounds (0, 0, PatchbayView::kWidth, PatchbayView::kHeight);
}
} // namespace swingers
