#include <catch2/catch_test_macros.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include "DrumMap.h"
#include "MapEditorView.h"
#include "MapLibrary.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

using namespace swingers;

namespace
{
struct TempRoot
{
    juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("swingers-mapedit-" + juce::Uuid().toString());
    ~TempRoot() { dir.deleteRecursively(); }
};
} // namespace

// ---------------------------------------------------------------------------------------------------------
// The editor view
// ---------------------------------------------------------------------------------------------------------
namespace
{
// A user map with a preferred kick and a snare, plus a second map to switch to.
void prepareRoot (const juce::File& root)
{
    DrumMap kit;
    kit.id = "my-kit";
    kit.name = "My Kit";
    kit.library = "Test";
    kit.notes = { { 36, "Kick", 36, true }, { 38, "Snare", 38, false } };
    DrumMap other;
    other.id = "other-kit";
    other.name = "Other Kit";
    other.notes = { { 50, "Tom", 50, false } };
    MapLibrary writer (root);
    REQUIRE (writer.saveMap (kit).wasOk());
    REQUIRE (writer.saveMap (other).wasOk());
}

juce::File fixture (const char* name)
{
    return juce::File (SWINGERS_FIXTURES_DIR).getChildFile (name);
}

juce::Button& button (MapEditorView& view, const char* id)
{
    auto* found = dynamic_cast<juce::Button*> (view.findChildWithID (id));
    REQUIRE (found != nullptr);
    return *found;
}

// Notes that come out when `note` goes in through the processor's current table.
std::vector<int> routed (SwingersProcessor& processor, int note)
{
    juce::MidiBuffer buffer;
    buffer.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
    processor.remapper().process (buffer);
    std::vector<int> out;
    for (const auto meta : buffer)
        if (meta.getMessage().isNoteOn())
            out.push_back (meta.getMessage().getNoteNumber());
    return out;
}

struct ViewFixture
{
    ViewFixture (const char* from = "general-midi", const char* to = "my-kit")
    {
        prepareRoot (tmp.dir);
        processor = std::make_unique<SwingersProcessor> (tmp.dir);
        processor->selectMaps (from, to);
        view = std::make_unique<MapEditorView> (*processor);
        view->open();
    }

    DrumMap saved (const char* id) const
    {
        MapLibrary fresh (tmp.dir);
        REQUIRE (fresh.find (id) != nullptr);
        return *fresh.find (id);
    }

    // Answers the unsaved-changes question with `choice` and counts the questions.
    void answerWith (MapEditorView::UnsavedChoice choice)
    {
        view->askUnsaved = [this, choice] (std::function<void (MapEditorView::UnsavedChoice)> reply) {
            ++questions;
            reply (choice);
        };
    }

    juce::ScopedJuceInitialiser_GUI gui;
    TempRoot tmp;
    std::unique_ptr<SwingersProcessor> processor;
    std::unique_ptr<MapEditorView> view;
    int questions = 0;
};
} // namespace

TEST_CASE ("map editor opens on the To map, or on From when To is General MIDI")
{
    {
        ViewFixture f ("other-kit", "my-kit");
        CHECK (f.view->editedMap().id == "my-kit");
        CHECK_FALSE (f.view->isDirty());
        CHECK_FALSE (f.view->isReadOnly());
        CHECK (f.view->getWidth() == 900);
        CHECK (f.view->getHeight() == 600);
    }
    {
        ViewFixture f ("other-kit", "general-midi");
        CHECK (f.view->editedMap().id == "other-kit");
    }
    {
        ViewFixture f ("general-midi", "general-midi");
        CHECK (f.view->editedMap().id == "general-midi");
    }
}

TEST_CASE ("editing a name and a GM voice and saving writes the file and rebuilds the table")
{
    ViewFixture f; // From = General MIDI, To = My Kit
    CHECK (routed (*f.processor, 37).empty()); // GM side stick: My Kit has no voice 37

    f.view->setNoteName (40, "Rimshot");
    f.view->setNoteGm (40, 37);
    CHECK (f.view->isDirty());
    CHECK (routed (*f.processor, 37).empty()); // nothing happens before SAVE

    REQUIRE (f.view->save());
    CHECK_FALSE (f.view->isDirty());

    const auto map = f.saved ("my-kit");
    REQUIRE (map.find (40) != nullptr);
    CHECK (map.find (40)->name == "Rimshot");
    CHECK (map.find (40)->gm == 37);
    CHECK (map.find (36)->preferred);
    CHECK (f.tmp.dir.getChildFile ("maps/my-kit.swmap.json").existsAsFile());

    CHECK (f.processor->current().to.find (40) != nullptr); // the processor rebuilt its table
    CHECK (routed (*f.processor, 37) == std::vector<int> { 40 });
}

TEST_CASE ("saving a map that is not in use leaves the processor alone")
{
    ViewFixture f; // To = My Kit
    f.view->loadMap ("other-kit");
    f.view->setNoteName (51, "Ride");
    f.view->setNoteGm (51, 51);
    REQUIRE (f.view->save());

    CHECK (f.saved ("other-kit").find (51) != nullptr);
    CHECK (f.processor->current().to.id == "my-kit");
}

TEST_CASE ("an empty name with no GM voice leaves the note out of the saved map")
{
    ViewFixture f;
    f.view->setNoteName (38, "");
    f.view->setNoteGm (38, kNoGm);
    f.view->setNoteName (60, "   "); // a note that was never in the map
    f.view->setNoteName (36, "");    // keeps its voice: stays, nameless
    REQUIRE (f.view->save());

    const auto map = f.saved ("my-kit");
    CHECK (map.find (38) == nullptr);
    CHECK (map.find (60) == nullptr);
    REQUIRE (map.find (36) != nullptr);
    CHECK (map.find (36)->name.isEmpty());
    CHECK (map.notes.size() == 1);
}

TEST_CASE ("the star is exclusive per GM voice and a lit star clears")
{
    ViewFixture f;
    f.view->setNoteName (40, "Snare 2");
    f.view->setNoteGm (40, 38);
    f.view->setNoteName (41, "Tom");
    f.view->setNoteGm (41, 41);

    f.view->togglePreferred (38);
    CHECK (f.view->isPreferred (38));
    f.view->togglePreferred (40); // same voice: takes the star away from 38
    CHECK (f.view->isPreferred (40));
    CHECK_FALSE (f.view->isPreferred (38));
    CHECK (f.view->isPreferred (36)); // another voice is not affected

    f.view->togglePreferred (40); // lit star: clears
    CHECK_FALSE (f.view->isPreferred (40));

    f.view->setNoteName (70, "Free");
    f.view->togglePreferred (70); // no GM voice: nothing to prefer
    CHECK_FALSE (f.view->isPreferred (70));

    f.view->togglePreferred (41);
    f.view->setNoteGm (41, kNoGm); // losing its voice loses the star
    CHECK_FALSE (f.view->isPreferred (41));
}

TEST_CASE ("bundled maps are read-only: fields and SAVE disabled, DUPLICATE enabled")
{
    ViewFixture f;
    f.view->loadMap (kGeneralMidiId);
    CHECK (f.view->isReadOnly());
    CHECK_FALSE (button (*f.view, "save").isEnabled());
    CHECK (button (*f.view, "duplicate").isEnabled());
    CHECK (f.view->hintText() == "Built-in map: duplicate it to edit");

    const auto original = f.view->editedMap().find (38)->name;
    f.view->setNoteName (38, "Hacked");
    f.view->setNoteGm (38, kNoGm);
    f.view->togglePreferred (38);
    CHECK_FALSE (f.view->isDirty());
    CHECK (f.view->editedMap().find (38)->name == original);
    CHECK_FALSE (f.view->save());

    // The row components follow.
    auto* row = f.view->table().getComponentForRowNumber (38);
    REQUIRE (row != nullptr);
    CHECK_FALSE (row->findChildWithID ("name")->isEnabled());
    CHECK_FALSE (row->findChildWithID ("gm")->isEnabled());
    CHECK_FALSE (f.view->findChildWithID ("mapName")->isEnabled());

    f.view->loadMap ("my-kit");
    CHECK_FALSE (f.view->isReadOnly());
    CHECK (button (*f.view, "save").isEnabled() == f.view->isDirty());
    row = f.view->table().getComponentForRowNumber (38);
    REQUIRE (row != nullptr);
    CHECK (row->findChildWithID ("name")->isEnabled());
    CHECK (row->findChildWithID ("gm")->isEnabled());
}

TEST_CASE ("the GM voice menu lists none and the GM names")
{
    ViewFixture f;
    auto* row = f.view->table().getComponentForRowNumber (36);
    REQUIRE (row != nullptr);
    auto* menu = dynamic_cast<juce::ComboBox*> (row->findChildWithID ("gm"));
    REQUIRE (menu != nullptr);
    CHECK (menu->getItemText (0) == "none");
    bool found = false;
    for (int i = 0; i < menu->getNumItems(); ++i)
        found = found || menu->getItemText (i) == "36  Bass Drum 1";
    CHECK (found);
    CHECK (menu->getText() == "36  Bass Drum 1");

    // Choosing from the menu edits the note.
    for (int i = 0; i < menu->getNumItems(); ++i)
        if (menu->getItemText (i) == "37  Side Stick")
            menu->setSelectedItemIndex (i, juce::sendNotificationSync);
    CHECK (f.view->editedMap().find (36)->gm == 37);
    CHECK (f.view->isDirty());
}

TEST_CASE ("duplicate makes an editable unsaved copy")
{
    ViewFixture f;
    button (*f.view, "duplicate").onClick();
    CHECK (f.view->editedMap().id.isEmpty()); // the id comes with the first save
    CHECK (f.view->editedMap().name == "My Kit (copy)");
    CHECK (f.view->isDirty()); // not on disk yet
    CHECK_FALSE (f.view->isReadOnly());
    CHECK (f.view->editedMap().notes.size() == 2);

    REQUIRE (f.view->save());
    const auto id = f.view->editedMap().id;
    CHECK (id.startsWith ("my-kit-copy-"));
    CHECK (id.length() == juce::String ("my-kit-copy-").length() + 4);
    CHECK (f.saved (id.toRawUTF8()).notes.size() == 2);
    CHECK (f.saved (id.toRawUTF8()).name == "My Kit (copy)");

    auto* selector = dynamic_cast<juce::ComboBox*> (f.view->findChildWithID ("mapSelector"));
    REQUIRE (selector != nullptr);
    CHECK (selector->getText() == "My Kit (copy)"); // the selector follows the new id

    // The same from a built-in map.
    f.view->loadMap (kGeneralMidiId);
    button (*f.view, "duplicate").onClick();
    CHECK (f.view->editedMap().name == "General MIDI (copy)");
    CHECK_FALSE (f.view->isReadOnly());
}

TEST_CASE ("new map starts empty and can be named and saved")
{
    ViewFixture f;
    button (*f.view, "new").onClick();
    CHECK (f.view->editedMap().id.isEmpty());
    CHECK (f.view->editedMap().name == "New map");
    CHECK (f.view->editedMap().notes.empty());

    f.view->setMapName ("Fresh Kit");
    f.view->setNoteName (36, "Boom");
    f.view->setNoteGm (36, 36);
    REQUIRE (f.view->save());

    const auto id = f.view->editedMap().id; // from the name at the first save
    CHECK (id.startsWith ("fresh-kit-"));
    CHECK (id.length() == juce::String ("fresh-kit-").length() + 4);
    const auto map = f.saved (id.toRawUTF8());
    CHECK (map.name == "Fresh Kit");
    CHECK (map.find (36)->name == "Boom");

    // Renamed and saved again: the id stays.
    f.view->setMapName ("Fresh Kit 2");
    REQUIRE (f.view->save());
    CHECK (f.view->editedMap().id == id);
    CHECK (f.saved (id.toRawUTF8()).name == "Fresh Kit 2");
}

TEST_CASE ("saving a new map never overwrites a map another instance saved with the same id meanwhile")
{
    ViewFixture f;
    f.processor->library().setRandomSeed (99);
    button (*f.view, "new").onClick();
    f.view->setMapName ("Studio Kit");
    f.view->setNoteName (36, "Boom");

    // The id the first draw would give, taken by another instance before SAVE.
    MapLibrary probe (f.tmp.dir);
    probe.setRandomSeed (99);
    const auto taken = probe.freshId ("Studio Kit");
    {
        MapLibrary other (f.tmp.dir);
        DrumMap theirs;
        theirs.id = taken;
        theirs.name = "Theirs";
        theirs.notes = { { 40, "Their Snare", 38, false } };
        REQUIRE (other.saveMap (theirs).wasOk());
    }

    REQUIRE (f.view->save());
    const auto id = f.view->editedMap().id;
    CHECK (id != taken);
    CHECK (id.startsWith ("studio-kit-"));
    CHECK (f.saved (taken.toRawUTF8()).name == "Theirs");
    CHECK (f.saved (id.toRawUTF8()).name == "Studio Kit");
    CHECK (f.tmp.dir.getChildFile ("maps/" + taken + ".swmap.json").existsAsFile());
    CHECK (f.tmp.dir.getChildFile ("maps/" + id + ".swmap.json").existsAsFile());
}

TEST_CASE ("unsaved changes: Discard, Save and Cancel")
{
    SECTION ("nothing to ask when nothing changed")
    {
        ViewFixture f;
        f.answerWith (MapEditorView::UnsavedChoice::cancel);
        button (*f.view, "duplicate").onClick();
        CHECK (f.questions == 0);
        CHECK (f.view->editedMap().name == "My Kit (copy)");
    }

    SECTION ("Discard drops the edits and goes on")
    {
        ViewFixture f;
        f.view->setNoteName (38, "Changed");
        f.answerWith (MapEditorView::UnsavedChoice::discard);
        f.view->loadMapGuarded ("other-kit");
        CHECK (f.questions == 1);
        CHECK (f.view->editedMap().id == "other-kit");
        CHECK_FALSE (f.view->isDirty());
        CHECK (f.saved ("my-kit").find (38)->name == "Snare");
    }

    SECTION ("Save writes the edits and goes on")
    {
        ViewFixture f;
        f.view->setNoteName (38, "Changed");
        f.answerWith (MapEditorView::UnsavedChoice::save);
        button (*f.view, "new").onClick();
        CHECK (f.questions == 1);
        CHECK (f.view->editedMap().name == "New map");
        CHECK (f.saved ("my-kit").find (38)->name == "Changed");
    }

    SECTION ("Cancel stays where it was, edits kept")
    {
        ViewFixture f;
        f.view->setNoteName (38, "Changed");
        f.answerWith (MapEditorView::UnsavedChoice::cancel);
        bool wentBack = false;
        f.view->onBack = [&] { wentBack = true; };
        button (*f.view, "back").onClick();
        CHECK (f.questions == 1);
        CHECK_FALSE (wentBack);
        CHECK (f.view->isDirty());
        CHECK (f.view->editedMap().find (38)->name == "Changed");
        f.view->loadMapGuarded ("other-kit");
        CHECK (f.view->editedMap().id == "my-kit");
    }

    SECTION ("BACK after Discard goes back")
    {
        ViewFixture f;
        f.view->setNoteName (38, "Changed");
        f.answerWith (MapEditorView::UnsavedChoice::discard);
        bool wentBack = false;
        f.view->onBack = [&] { wentBack = true; };
        button (*f.view, "back").onClick();
        CHECK (wentBack);
        CHECK (f.saved ("my-kit").find (38)->name == "Snare");
    }

    SECTION ("a failed Save does not go on")
    {
        ViewFixture f;
        f.view->setNoteName (38, "Changed");
        f.tmp.dir.getChildFile ("maps").deleteRecursively();
        REQUIRE (f.tmp.dir.getChildFile ("maps").replaceWithText ("in the way")); // a file where the folder should be
        f.answerWith (MapEditorView::UnsavedChoice::save);
        button (*f.view, "new").onClick();
        CHECK (f.view->editedMap().id == "my-kit");
        CHECK (f.view->isDirty());
        CHECK (f.view->statusIsError());
    }
}

TEST_CASE ("a failed save shows an amber status and keeps the edits")
{
    ViewFixture f;
    f.view->setNoteName (38, "Changed");
    f.tmp.dir.getChildFile ("maps").deleteRecursively();
    REQUIRE (f.tmp.dir.getChildFile ("maps").replaceWithText ("in the way"));

    CHECK_FALSE (f.view->save());
    CHECK (f.view->statusIsError());
    CHECK (f.view->statusText().isNotEmpty());
    CHECK (f.view->isDirty());
    CHECK (f.view->editedMap().find (38)->name == "Changed");
}

TEST_CASE ("import from Cubase selects the new map; a bad file reports in the status line")
{
    ViewFixture f;
    f.view->importFile (fixture ("cubase_unescaped.xml"));
    CHECK_FALSE (f.view->statusIsError());
    CHECK (f.view->editedMap().name == "Odd Kit");
    CHECK_FALSE (f.view->isReadOnly());
    CHECK_FALSE (f.view->isDirty());
    CHECK (f.processor->library().find (f.view->editedMap().id) != nullptr);
    CHECK (f.view->editedMap().find (84) != nullptr); // an imported note

    const auto id = f.view->editedMap().id;
    f.view->importFile (f.tmp.dir.getChildFile ("missing.xml"));
    CHECK (f.view->statusIsError());
    CHECK (f.view->editedMap().id == id); // still on the previous map

    f.view->importFile (fixture ("cubase_unescaped.xml")); // a second import gets its own id
    CHECK (f.view->editedMap().id != id);
}

TEST_CASE ("the selector lists the library; the editor opens the view and BACK returns")
{
    ViewFixture f;
    auto* selector = dynamic_cast<juce::ComboBox*> (f.view->findChildWithID ("mapSelector"));
    REQUIRE (selector != nullptr);
    CHECK (selector->getNumItems() == 3);
    CHECK (selector->getText() == "My Kit");

    SwingersProcessor processor2 (f.tmp.dir);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor2.createEditor());
    auto* swingersEditor = dynamic_cast<SwingersEditor*> (editor.get());
    REQUIRE (swingersEditor != nullptr);
    swingersEditor->showPatchbay (true);
    auto* editMaps = dynamic_cast<juce::Button*> (swingersEditor->patchbayView().findChildWithID ("editMaps"));
    REQUIRE (editMaps != nullptr);
    CHECK (editMaps->isEnabled());
    editMaps->onClick();
    CHECK (swingersEditor->isMapEditorShown());
    CHECK_FALSE (swingersEditor->isPatchbayShown());
    CHECK (editor->getWidth() == 900);
    CHECK (editor->getHeight() == 600);
    button (swingersEditor->mapEditorView(), "back").onClick();
    CHECK_FALSE (swingersEditor->isMapEditorShown());
    CHECK (swingersEditor->isPatchbayShown());
}

TEST_CASE ("text typed in a row reaches the map, also when SAVE comes before the change message")
{
    ViewFixture f;
    auto* row = f.view->table().getComponentForRowNumber (38);
    REQUIRE (row != nullptr);
    auto* field = dynamic_cast<juce::TextEditor*> (row->findChildWithID ("name"));
    REQUIRE (field != nullptr);

    field->setText ("Snare Typed", false); // as typing does, but the change message is still queued
    CHECK (f.view->editedMap().find (38)->name == "Snare");
    REQUIRE (f.view->save());
    CHECK (f.saved ("my-kit").find (38)->name == "Snare Typed");

    // The message path.
    field->setText ("Snare Again", true);
    const auto end = juce::Time::getMillisecondCounter() + 100;
    while (juce::Time::getMillisecondCounter() < end)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
    CHECK (f.view->editedMap().find (38)->name == "Snare Again");
    CHECK (f.view->isDirty());
}

TEST_CASE ("unsaved edits survive the editor window being closed")
{
    juce::ScopedJuceInitialiser_GUI gui;
    TempRoot tmp;
    prepareRoot (tmp.dir);
    SwingersProcessor processor (tmp.dir);
    processor.selectMaps ("general-midi", "my-kit");

    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        auto* swingersEditor = dynamic_cast<SwingersEditor*> (editor.get());
        REQUIRE (swingersEditor != nullptr);
        swingersEditor->showMapEditor (true);
        swingersEditor->mapEditorView().setNoteName (38, "Snare Draft");
        CHECK (swingersEditor->mapEditorView().isDirty());
    } // the host closes the window
    CHECK (processor.hasMapDraft());

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    auto* swingersEditor = dynamic_cast<SwingersEditor*> (editor.get());
    REQUIRE (swingersEditor != nullptr);
    CHECK (swingersEditor->isMapEditorShown());
    CHECK (editor->getWidth() == 900);
    CHECK (editor->getHeight() == 600);
    CHECK_FALSE (processor.hasMapDraft()); // consumed

    auto& view = swingersEditor->mapEditorView();
    CHECK (view.isDirty());
    CHECK (view.editedMap().id == "my-kit");
    CHECK (view.editedMap().find (38)->name == "Snare Draft");
    CHECK (view.statusText() == "Unsaved changes restored");
    CHECK_FALSE (view.statusIsError());
    CHECK (button (view, "save").isEnabled());

    REQUIRE (view.save());
    MapLibrary fresh (tmp.dir);
    CHECK (fresh.find ("my-kit")->find (38)->name == "Snare Draft");

    // Saved: closing again leaves no draft.
    editor.reset();
    CHECK_FALSE (processor.hasMapDraft());
}

TEST_CASE ("a draft of a new map comes back as a new map; a discarded edit leaves no draft")
{
    juce::ScopedJuceInitialiser_GUI gui;
    TempRoot tmp;
    prepareRoot (tmp.dir);
    SwingersProcessor processor (tmp.dir);

    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        auto* swingersEditor = dynamic_cast<SwingersEditor*> (editor.get());
        swingersEditor->showMapEditor (true);
        auto& view = swingersEditor->mapEditorView();
        view.startNewMap();
        view.setMapName ("Draft Kit");
        view.setNoteName (36, "Boom");
    }
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        auto* swingersEditor = dynamic_cast<SwingersEditor*> (editor.get());
        REQUIRE (swingersEditor->isMapEditorShown());
        auto& view = swingersEditor->mapEditorView();
        CHECK (view.editedMap().id.isEmpty());
        CHECK (view.editedMap().name == "Draft Kit");
        CHECK (view.editedMap().find (36)->name == "Boom");
        CHECK_FALSE (button (view, "duplicate").isEnabled()); // nothing in the library to copy yet
        CHECK (processor.library().maps().size() == 3); // GM, My Kit, Other Kit: nothing saved

        // Discard through the public API: asks, then leaves no draft behind.
        int questions = 0;
        view.askUnsaved = [&questions] (std::function<void (MapEditorView::UnsavedChoice)> reply) {
            ++questions;
            reply (MapEditorView::UnsavedChoice::discard);
        };
        swingersEditor->showMapEditor (false);
        CHECK (questions == 1);
        CHECK (swingersEditor->isPatchbayShown());
        CHECK_FALSE (swingersEditor->isMapEditorShown());
    }
    CHECK_FALSE (processor.hasMapDraft());
}

TEST_CASE ("leaving the map editor through the editor API asks about unsaved edits")
{
    juce::ScopedJuceInitialiser_GUI gui;
    TempRoot tmp;
    prepareRoot (tmp.dir);
    SwingersProcessor processor (tmp.dir);
    processor.selectMaps ("general-midi", "my-kit");
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    auto* swingersEditor = dynamic_cast<SwingersEditor*> (editor.get());
    swingersEditor->showMapEditor (true);
    auto& view = swingersEditor->mapEditorView();
    view.setNoteName (38, "Changed");

    int questions = 0;
    view.askUnsaved = [&questions] (std::function<void (MapEditorView::UnsavedChoice)> reply) {
        ++questions;
        reply (MapEditorView::UnsavedChoice::cancel);
    };
    swingersEditor->showMapEditor (false);
    swingersEditor->showPatchbay (false);
    swingersEditor->showPatchbay (true);
    CHECK (questions == 3);
    CHECK (swingersEditor->isMapEditorShown());
    CHECK (view.isDirty());
    CHECK (view.editedMap().find (38)->name == "Changed");

    swingersEditor->showMapEditor (true); // already shown: keeps the edits
    CHECK (view.editedMap().find (38)->name == "Changed");
}

TEST_CASE ("the selector follows its own list of ids")
{
    ViewFixture f;
    auto* selector = dynamic_cast<juce::ComboBox*> (f.view->findChildWithID ("mapSelector"));
    REQUIRE (selector != nullptr);
    int otherIndex = -1;
    for (int i = 0; i < selector->getNumItems(); ++i)
        if (selector->getItemText (i) == "Other Kit")
            otherIndex = i;
    REQUIRE (otherIndex >= 0);

    // A map saved elsewhere meanwhile shifts the library order; the item still means "Other Kit".
    DrumMap early;
    early.id = "aaa-kit";
    early.name = "AAA Kit";
    early.notes = { { 36, "K", 36, false } };
    REQUIRE (f.processor->library().saveMap (early).wasOk());
    selector->setSelectedItemIndex (otherIndex, juce::sendNotificationSync);
    CHECK (f.view->editedMap().id == "other-kit");
}
