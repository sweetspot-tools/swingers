#include "PluginEditor.h"

namespace swingers
{
SwingersEditor::SwingersEditor (SwingersProcessor& p)
    : juce::AudioProcessorEditor (p), processor (p), compact (p), patchbay (p), mapEditor (p)
{
    setLookAndFeel (&lookAndFeel);
    setResizable (false, false);

    addAndMakeVisible (wearBackground);
    addAndMakeVisible (compact);
    addChildComponent (patchbay);
    addChildComponent (mapEditor);
    addAndMakeVisible (wearTape);
    wearBackground.setComponentID ("wearBackground");
    wearTape.setComponentID ("wearTape");
    wearBackground.setSeed (processor.wearSeed());
    wearTape.setSeed (processor.wearSeed());
    setWearKeepClear (CompactPanel::wearKeepClear());
    compact.setComponentID ("compactPanel");
    patchbay.setComponentID ("patchbayView");
    mapEditor.setComponentID ("mapEditorView");
    compact.onEditMapping = [this] { showPatchbay (true); };
    patchbay.onDone = [this] { showPatchbay (false); };
    patchbay.setEditMapsHandler ([this] { showMapEditor (true); });
    mapEditor.onBack = [this] { applyPatchbay (true); }; // the view has asked already
    mapEditor.onLibraryChanged = [this] { compact.refresh(); };
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

    // The previous editor window was closed with unsaved map edits: come back to them.
    if (auto draft = processor.takeMapDraft())
    {
        mapEditor.restoreDraft (*draft);
        enterMapEditor();
    }
}

SwingersEditor::~SwingersEditor()
{
    if (mapEditorShown)
        if (auto draft = mapEditor.draft())
            processor.setMapDraft (std::move (*draft));

    processor.onStateChanged = nullptr;
    patchbay.onLayoutChanged = nullptr;
    patchbay.setEditMapsHandler (nullptr);
    mapEditor.onBack = nullptr;
    mapEditor.onLibraryChanged = nullptr;
    setLookAndFeel (nullptr);
}

void SwingersEditor::showPatchbay (bool show)
{
    if (mapEditorShown)
        mapEditor.runGuarded ([this, show] { applyPatchbay (show); });
    else
        applyPatchbay (show);
}

void SwingersEditor::applyPatchbay (bool show)
{
    patchbayShown = show;
    mapEditorShown = false;
    mapEditor.setVisible (false);
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

void SwingersEditor::showMapEditor (bool show)
{
    if (! show)
    {
        showPatchbay (true);
        return;
    }

    if (mapEditorShown)
        return; // already there: opening again would drop the edits
    mapEditor.open();
    enterMapEditor();
}

void SwingersEditor::enterMapEditor()
{
    patchbayShown = false;
    mapEditorShown = true;
    compact.setVisible (false);
    patchbay.setVisible (false);
    mapEditor.setVisible (true);
    setWearKeepClear (MapEditorView::wearKeepClear());
    setSize (MapEditorView::kWidth, MapEditorView::kHeight);
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
    mapEditor.setBounds (0, 0, MapEditorView::kWidth, MapEditorView::kHeight);
}
} // namespace swingers
