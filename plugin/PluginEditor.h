#pragma once

#include "CompactPanel.h"
#include "PluginProcessor.h"
#include "RackLookAndFeel.h"

namespace swingers
{
// Editor: the compact From/To panel. Later tasks add the patchbay view and the standalone file bar.
class SwingersEditor : public juce::AudioProcessorEditor
{
public:
    explicit SwingersEditor (SwingersProcessor&);
    ~SwingersEditor() override;

    // Placeholder until the patchbay exists: only remembers the request.
    void showPatchbay (bool show);
    bool isPatchbayShown() const { return patchbayShown; }

    CompactPanel& compactPanel() { return compact; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    SwingersProcessor& processor;
    RackLookAndFeel lookAndFeel; // declared before the children that use it
    CompactPanel compact;
    juce::TooltipWindow tooltips { this, 600 };
    bool patchbayShown = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwingersEditor)
};
} // namespace swingers
