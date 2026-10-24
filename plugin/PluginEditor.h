#pragma once

#include "CompactPanel.h"
#include "PluginProcessor.h"
#include "RackLookAndFeel.h"
#include "WearLayer.h"

#include <vector>

namespace swingers
{
// Editor: the compact From/To panel, between two wear layers (panel texture behind, tape in front). Later tasks add the patchbay view and the standalone file bar.
class SwingersEditor : public juce::AudioProcessorEditor
{
public:
    explicit SwingersEditor (SwingersProcessor&);
    ~SwingersEditor() override;

    // Placeholder until the patchbay exists: only remembers the request.
    void showPatchbay (bool show);
    bool isPatchbayShown() const { return patchbayShown; }

    CompactPanel& compactPanel() { return compact; }

    // Rectangles the tape pieces and stains must stay clear of (editor coordinates). The compact panel sets
    // its own; the patchbay, map editor and file bar pass theirs when they are shown.
    void setWearKeepClear (const std::vector<juce::Rectangle<int>>&);

    void resized() override;

private:
    SwingersProcessor& processor;
    RackLookAndFeel lookAndFeel; // declared before the children that use it
    WearLayer wearBackground { WearLayer::Layer::Background }; // behind everything
    CompactPanel compact;
    WearLayer wearTape { WearLayer::Layer::Tape }; // above everything, clear of the controls
    juce::TooltipWindow tooltips { this, 600 };
    bool patchbayShown = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwingersEditor)
};
} // namespace swingers
