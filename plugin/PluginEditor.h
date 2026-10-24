#pragma once

#include "CompactPanel.h"
#include "MapEditorView.h"
#include "PatchbayView.h"
#include "PluginProcessor.h"
#include "RackLookAndFeel.h"
#include "WearLayer.h"

#include <vector>

namespace swingers
{
// Editor: the compact From/To panel and the patchbay, between two wear layers (panel texture behind, tape in front). Later tasks add the patchbay view and the standalone file bar.
class SwingersEditor : public juce::AudioProcessorEditor
{
public:
    explicit SwingersEditor (SwingersProcessor&);
    ~SwingersEditor() override;

    // true: the editor grows to the patchbay (900 x 600) and hides the compact panel; false: back to 520 x 140.
    void showPatchbay (bool show);
    bool isPatchbayShown() const { return patchbayShown; }

    // true: the map editor (900 x 600) replaces the patchbay; false: back to the patchbay.
    void showMapEditor (bool show);
    bool isMapEditorShown() const { return mapEditorShown; }
    MapEditorView& mapEditorView() { return mapEditor; }

    CompactPanel& compactPanel() { return compact; }
    PatchbayView& patchbayView() { return patchbay; }

    // Rectangles the tape pieces and stains must stay clear of (editor coordinates). The compact panel sets
    // its own; the patchbay, map editor and file bar pass theirs when they are shown.
    void setWearKeepClear (const std::vector<juce::Rectangle<int>>&);

    void resized() override;

private:
    SwingersProcessor& processor;
    RackLookAndFeel lookAndFeel; // declared before the children that use it
    WearLayer wearBackground { WearLayer::Layer::Background }; // behind everything
    CompactPanel compact;
    PatchbayView patchbay; // hidden until Edit mapping
    MapEditorView mapEditor; // hidden until Edit maps
    WearLayer wearTape { WearLayer::Layer::Tape }; // above everything, clear of the controls
    juce::TooltipWindow tooltips { this, 600 };
    bool patchbayShown = false;
    bool mapEditorShown = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwingersEditor)
};
} // namespace swingers
