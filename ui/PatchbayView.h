#pragma once

#include "PatchbayModel.h"
#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace swingers
{
class PatchbayCanvas;

// "Edit mapping": source notes on the left, GM voices in the middle (quiet), target notes on the right, with
// the cables between them. Message thread only. Gestures edit the exceptions of the pair in use through
// SwingersProcessor::applyOverrides; the view redraws when the editor calls refresh() (onStateChanged).
class PatchbayView : public juce::Component, private juce::Timer
{
public:
    static constexpr int kWidth = 900;
    static constexpr int kHeight = 600;
    static constexpr uint32_t kLitMs = 120; // a note and its cable stay lit this long after a routed note-on

    explicit PatchbayView (SwingersProcessor&);
    ~PatchbayView() override;

    std::function<void()> onDone;
    std::function<void()> onLayoutChanged;  // the boxes changed height: the editor re-reads wearKeepClear()

    // Task 11: what EDIT MAPS does. The button looks disabled while there is no handler.
    void setEditMapsHandler (std::function<void()>);

    // Rebuilds the model from the processor's current maps and exceptions.
    void refresh();

    void setAllNotes (bool);
    bool isAllNotes() const;
    const PatchbayModel& model() const { return patchModel; }

    // What the gestures do (the canvas calls these): each one saves the pair's exceptions through the processor.
    void connectNotes (int sourceNote, int targetNote); // direct cable
    void disconnectNote (int sourceNote);               // dropped by the user
    void resetNote (int sourceNote);                    // back to the standard path

    // 30 Hz timer body: a source note lights up for kLitMs when remapper().hits(note) changed. No allocation.
    void pollHits (uint32_t nowMs);
    bool isLit (int sourceNote) const;

    // Rows and the canvas, for tests: bounds are in canvas coordinates (the scrolled content).
    juce::Component& canvas();
    juce::Viewport& viewport() { return scroller; }
    int gmPillCentreY (int voice) const;                   // canvas y of the voice's pill, -1 if none
    juce::Rectangle<int> sourceRowBounds (int note) const; // empty if the note has no row
    juce::Rectangle<int> targetRowBounds (int note) const;
    juce::Rectangle<int> boxesArea() const;                // both boxes and the gap, view coordinates

    // Where the wear must stay clear: boxes with the cables, title, buttons, hint line, scroll bar.
    std::vector<juce::Rectangle<int>> wearKeepClear() const;

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

private:
    void timerCallback() override;
    void layoutContent();
    void updateEditMapsEnabled();
    void edit (const std::function<void (PairOverrides&)>&);

    SwingersProcessor& processor;
    PatchbayModel patchModel;
    bool allNotes = false;
    std::function<void()> editMapsHandler;

    juce::TextButton allNotesButton { "All notes" };
    juce::TextButton editMapsButton { "Edit maps" };
    juce::TextButton doneButton { "Done" };
    juce::Viewport scroller;
    std::unique_ptr<PatchbayCanvas> canvasComponent;
    int boxHeight = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatchbayView)
};
} // namespace swingers
