#pragma once

#include "CompactPanel.h"
#include "FileBar.h"
#include "MapEditorView.h"
#include "PatchbayView.h"
#include "PluginProcessor.h"
#include "RackLookAndFeel.h"
#include "WearLayer.h"

#include <vector>

namespace swingers
{
// Editor: the compact From/To panel, the patchbay, the map editor and (standalone only) the file bar, between two wear layers (panel texture behind, tape in front).
class SwingersEditor : public juce::AudioProcessorEditor
{
public:
    explicit SwingersEditor (SwingersProcessor&);
    ~SwingersEditor() override;

    // true: the editor grows to the patchbay (900 x 600) and hides the compact panel; false: back to the compact size (520 x 140, or 520 x 330 with the file bar).
    // Leaving the map editor with unsaved edits asks first (Save / Discard / Cancel); the change may happen later.
    void showPatchbay (bool show);
    bool isPatchbayShown() const { return patchbayShown; }

    // true: the map editor (900 x 600) replaces the patchbay; false: back to the patchbay.
    void showMapEditor (bool show);
    bool isMapEditorShown() const { return mapEditorShown; }
    MapEditorView& mapEditorView() { return mapEditor; }

    CompactPanel& compactPanel() { return compact; }

    // The standalone file bar (OPEN MIDI / CONVERT / SAVE and the report) under the compact panel. Shown by
    // default only when the processor runs as the standalone app; tests and the render tool can force it.
    // The compact size is then 520 x 330 instead of 520 x 140.
    void setFileBarShown (bool show);
    bool isFileBarShown() const { return fileBarShown; }
    FileBar& fileBarView() { return fileBar; }
    PatchbayView& patchbayView() { return patchbay; }

    // Rectangles the tape pieces and stains must stay clear of (editor coordinates). The compact panel sets
    // its own; the patchbay, map editor and file bar pass theirs when they are shown.
    void setWearKeepClear (const std::vector<juce::Rectangle<int>>&);

    void resized() override;

private:
    void applyPatchbay (bool show);  // showPatchbay without the question
    void enterMapEditor();           // shows the map editor as it is
    std::vector<juce::Rectangle<int>> compactKeepClear() const;
    int compactHeight() const { return fileBarShown ? FileBar::kHeight : CompactPanel::kHeight; }

    SwingersProcessor& processor;
    RackLookAndFeel lookAndFeel; // declared before the children that use it
    WearLayer wearBackground { WearLayer::Layer::Background }; // behind everything
    CompactPanel compact;
    PatchbayView patchbay; // hidden until Edit mapping
    MapEditorView mapEditor; // hidden until Edit maps
    FileBar fileBar; // standalone only; above the compact panel's silkscreen, below the tape
    WearLayer wearTape { WearLayer::Layer::Tape }; // above everything, clear of the controls
    juce::TooltipWindow tooltips { this, 600 };
    bool patchbayShown = false;
    bool mapEditorShown = false;
    bool fileBarShown = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwingersEditor)
};
} // namespace swingers
