#include <catch2/catch_test_macros.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include "CompactPanel.h"
#include "MapLibrary.h"
#include "PatchbayView.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "WearLayer.h"

using namespace swingers;

namespace
{
struct TempRoot
{
    juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("swingers-patchbay-" + juce::Uuid().toString());
    ~TempRoot() { dir.deleteRecursively(); }
};

// Two user maps: a source kit and a target kit with one preferred note.
void prepareRoot (const juce::File& root)
{
    DrumMap from;
    from.id = "src-kit";
    from.name = "Src Kit";
    from.notes = { { 23, "HH Open 2", 46, false }, { 36, "Kick", 36, false }, { 40, "Flam", kNoGm, false } };
    DrumMap to;
    to.id = "dst-kit";
    to.name = "Dst Kit";
    to.notes = { { 26, "HH Open", 46, false }, { 30, "Other", 49, false }, { 33, "Kick R", 36, true } };
    MapLibrary writer (root);
    REQUIRE (writer.saveMap (from).wasOk());
    REQUIRE (writer.saveMap (to).wasOk());
}

juce::MouseEvent mouse (juce::Component& c, juce::Point<float> position, juce::Point<float> downPosition,
                        int buttonFlag, bool dragged)
{
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), position,
                             juce::ModifierKeys (buttonFlag), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, now, downPosition,
                             now, 1, dragged);
}

// Left-button drag in the canvas, from `from` to `to` (canvas coordinates).
void drag (juce::Component& canvas, juce::Point<float> from, juce::Point<float> to)
{
    constexpr auto left = juce::ModifierKeys::leftButtonModifier;
    canvas.mouseDown (mouse (canvas, from, from, left, false));
    canvas.mouseDrag (mouse (canvas, to, from, left, true));
    canvas.mouseUp (mouse (canvas, to, from, left, true));
}

juce::Point<float> portOf (const juce::Rectangle<int>& row)
{
    return { (float) row.getRight() - 24.0f, (float) row.getCentreY() };
}

void pumpMessages()
{
    const auto end = juce::Time::getMillisecondCounter() + 100;
    while (juce::Time::getMillisecondCounter() < end)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
}

struct Fixture
{
    juce::ScopedJuceInitialiser_GUI gui;
    TempRoot tmp;
    std::unique_ptr<SwingersProcessor> processor;
    std::unique_ptr<juce::AudioProcessorEditor> editorHolder;
    SwingersEditor* editor = nullptr;

    Fixture()
    {
        prepareRoot (tmp.dir);
        processor = std::make_unique<SwingersProcessor> (tmp.dir);
        processor->selectMaps ("src-kit", "dst-kit");
        editorHolder.reset (processor->createEditor());
        editor = dynamic_cast<SwingersEditor*> (editorHolder.get());
        REQUIRE (editor != nullptr);
    }

    PatchbayView& view() { return editor->patchbayView(); }
};
} // namespace

TEST_CASE ("patchbay editor: Edit mapping grows the editor to 900 x 600 and Done shrinks it back")
{
    Fixture f;
    CHECK (f.editor->getWidth() == 520);
    CHECK (f.editor->getHeight() == 140);
    CHECK_FALSE (f.view().isVisible());

    auto* background = dynamic_cast<WearLayer*> (f.editor->findChildWithID ("wearBackground"));
    REQUIRE (background != nullptr);

    f.editor->compactPanel().onEditMapping();
    CHECK (f.editor->isPatchbayShown());
    CHECK (f.editor->getWidth() == 900);
    CHECK (f.editor->getHeight() == 600);
    CHECK (f.view().isVisible());
    CHECK_FALSE (f.editor->compactPanel().isVisible());
    CHECK (background->getWidth() == 900);
    CHECK (background->getKeepClear() == f.view().wearKeepClear());

    auto* done = dynamic_cast<juce::Button*> (f.view().findChildWithID ("done"));
    REQUIRE (done != nullptr);
    done->triggerClick();
    pumpMessages();

    CHECK_FALSE (f.editor->isPatchbayShown());
    CHECK (f.editor->getWidth() == 520);
    CHECK (f.editor->getHeight() == 140);
    CHECK_FALSE (f.view().isVisible());
    CHECK (f.editor->compactPanel().isVisible());
    CHECK (background->getKeepClear() == CompactPanel::wearKeepClear());
}

TEST_CASE ("patchbay editor: the wear keeps clear of the boxes, the buttons and the title")
{
    Fixture f;
    f.editor->showPatchbay (true);
    for (const auto& r : f.view().wearKeepClear())
    {
        CHECK (juce::Rectangle<int> (0, 0, 900, 600).contains (r));
    }
    const auto keep = f.view().wearKeepClear();
    CHECK (keep.front().contains (f.view().boxesArea()));
}

TEST_CASE ("patchbay: the view lists the rows and cables of the pair in use")
{
    Fixture f;
    f.editor->showPatchbay (true);
    const auto& model = f.view().model();
    REQUIRE (model.sources.size() == 3);
    REQUIRE (model.targets.size() == 3);
    CHECK (model.gmVoices == std::vector<int> { 36, 46 });
    CHECK (model.cables.size() == 2); // 40 has no GM voice
    CHECK_FALSE (f.view().sourceRowBounds (23).isEmpty());
    CHECK_FALSE (f.view().targetRowBounds (26).isEmpty());
    CHECK (f.view().sourceRowBounds (99).isEmpty());
    CHECK (f.view().targetRowBounds (23).isEmpty());

    // No scroll bar for a handful of rows.
    CHECK (f.view().viewport().getMaximumVisibleHeight() >= f.view().canvas().getHeight());
}

TEST_CASE ("patchbay: connect, disconnect and reset reach the processor and the pair file")
{
    Fixture f;
    f.editor->showPatchbay (true);
    const auto pairFile = f.tmp.dir.getChildFile ("pairs/src-kit__dst-kit.swpair.json");
    CHECK_FALSE (pairFile.existsAsFile());

    f.view().connectNotes (23, 30);
    CHECK (f.processor->current().overrides.direct.at (23) == 30);
    CHECK (f.processor->current().overrides.fromId == "src-kit");
    CHECK (f.processor->current().overrides.toId == "dst-kit");
    CHECK (pairFile.existsAsFile());
    CHECK (f.view().model().cables.size() == 2);
    bool direct = false;
    for (const auto& c : f.view().model().cables)
        direct = direct || (c.source == 23 && c.direct && c.target == 30);
    CHECK (direct); // the view followed onStateChanged

    f.view().disconnectNote (36);
    CHECK (f.processor->current().overrides.dropped.count (36) == 1);
    CHECK (f.processor->current().overrides.direct.at (23) == 30); // kept
    REQUIRE (f.view().model().findSource (36) != nullptr);
    CHECK (f.view().model().findSource (36)->state == SourceState::DroppedByUser);

    f.view().resetNote (36);
    f.view().resetNote (23);
    CHECK (f.processor->current().overrides.empty());
    CHECK_FALSE (pairFile.existsAsFile());
    CHECK (f.view().model().cables.size() == 2);
}

TEST_CASE ("patchbay: dragging a source row onto a target row connects it")
{
    Fixture f;
    f.editor->showPatchbay (true);
    auto& canvas = f.view().canvas();

    const auto source = f.view().sourceRowBounds (23);
    const auto target = f.view().targetRowBounds (30);
    drag (canvas, portOf (source), target.getCentre().toFloat());

    REQUIRE (f.processor->current().overrides.direct.count (23) == 1);
    CHECK (f.processor->current().overrides.direct.at (23) == 30);
    CHECK (f.view().model().findSource (23)->state == SourceState::Connected);
}

TEST_CASE ("patchbay: releasing a dragged cable in empty space disconnects the note, a plain click does nothing")
{
    Fixture f;
    f.editor->showPatchbay (true);
    auto& canvas = f.view().canvas();
    const auto kick = f.view().sourceRowBounds (36);

    // Click without moving.
    constexpr auto left = juce::ModifierKeys::leftButtonModifier;
    canvas.mouseDown (mouse (canvas, portOf (kick), portOf (kick), left, false));
    canvas.mouseUp (mouse (canvas, portOf (kick), portOf (kick), left, false));
    CHECK (f.processor->current().overrides.empty());

    // Dragged into the gap between the columns.
    drag (canvas, portOf (kick), { 450.0f, 300.0f });
    CHECK (f.processor->current().overrides.dropped.count (36) == 1);
    CHECK (f.view().model().findSource (36)->state == SourceState::DroppedByUser);

    // A dropped row does not start a cable.
    const auto dropped = f.view().sourceRowBounds (36);
    drag (canvas, portOf (dropped), f.view().targetRowBounds (33).getCentre().toFloat());
    CHECK (f.processor->current().overrides.direct.count (36) == 0);
    CHECK (f.processor->current().overrides.dropped.count (36) == 1);
}

TEST_CASE ("patchbay: a source row lights up for 120 ms when its note is routed")
{
    Fixture f;
    f.editor->showPatchbay (true);
    auto& view = f.view();

    view.pollHits (1000);
    CHECK_FALSE (view.isLit (36));

    juce::AudioBuffer<float> audio (0, 16);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 0);
    f.processor->processBlock (audio, midi);

    view.pollHits (1010);
    CHECK (view.isLit (36));
    CHECK_FALSE (view.isLit (23));
    view.pollHits (1010 + 119);
    CHECK (view.isLit (36));
    view.pollHits (1010 + 120);
    CHECK_FALSE (view.isLit (36));

    // Played while the view was hidden: no flash when it shows up again.
    juce::MidiBuffer more;
    more.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 0);
    f.processor->processBlock (audio, more);
    f.editor->showPatchbay (false);
    f.editor->showPatchbay (true);
    view.pollHits (5000);
    CHECK_FALSE (view.isLit (36));
}

TEST_CASE ("patchbay: All notes shows 128 rows and scrolls")
{
    Fixture f;
    f.editor->showPatchbay (true);
    auto& view = f.view();

    auto* toggle = dynamic_cast<juce::Button*> (view.findChildWithID ("allNotes"));
    REQUIRE (toggle != nullptr);
    CHECK_FALSE (toggle->getToggleState());

    toggle->setToggleState (true, juce::sendNotificationSync);
    CHECK (view.isAllNotes());
    CHECK (view.model().sources.size() == 128);
    CHECK (view.model().targets.size() == 128);
    CHECK (view.canvas().getHeight() > view.viewport().getHeight());
    CHECK (view.viewport().getHeight() <= 500);
    CHECK (f.editor->findChildWithID ("wearBackground") != nullptr);

    // It starts at the first note the source map defines (23), and scrolling moves the canvas, not the boxes.
    CHECK (view.sourceRowBounds (0).getY() < view.sourceRowBounds (127).getY());
    CHECK (view.viewport().getViewPositionY() > 0);
    CHECK (view.viewport().getViewPositionY() <= view.sourceRowBounds (23).getY());
    view.viewport().setViewPosition (0, 1000);
    CHECK (view.viewport().getViewPositionY() == 1000);

    toggle->setToggleState (false, juce::sendNotificationSync);
    CHECK (view.model().sources.size() == 3);
}

TEST_CASE ("patchbay: Edit maps is disabled until something handles it")
{
    Fixture f;
    auto* button = dynamic_cast<juce::Button*> (f.view().findChildWithID ("editMaps"));
    REQUIRE (button != nullptr);

    f.editor->showPatchbay (true);
    CHECK_FALSE (button->isEnabled());

    int calls = 0;
    f.view().onEditMaps = [&calls] { ++calls; };
    f.editor->showPatchbay (false);
    f.editor->showPatchbay (true);
    CHECK (button->isEnabled());
    button->triggerClick();
    pumpMessages();
    CHECK (calls == 1);
}

TEST_CASE ("patchbay: follows maps chosen elsewhere while it is shown")
{
    Fixture f;
    f.editor->showPatchbay (true);
    REQUIRE (f.view().model().sources.size() == 3);

    f.processor->selectMaps ("dst-kit", "src-kit");
    CHECK (f.view().model().sources.size() == 3);
    CHECK (f.view().model().sources.front().note == 26);

    f.processor->selectMaps ("general-midi", "dst-kit");
    CHECK_FALSE (f.view().model().showGmColumn);
    CHECK (f.view().model().sources.size() > 40); // the GM map
}
