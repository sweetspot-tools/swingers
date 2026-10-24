#include <catch2/catch_test_macros.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include "FileBar.h"
#include "MapLibrary.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <cstring>
#include <memory>
#include <vector>

using namespace swingers;

namespace
{
struct TempRoot
{
    juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("swingers-filebar-" + juce::Uuid().toString());
    ~TempRoot() { dir.deleteRecursively(); }
};

// Source kit: 23 -> HH Open (GM 46), 36 Kick, 37 Snare Rim, 61 Splash FX (no GM voice: no match).
// Target kit: 26 HH Open, 36 Kick R. The pair drops 37 on purpose.
void prepareProcessor (const juce::File& root)
{
    DrumMap from;
    from.id = "src-kit";
    from.name = "Src Kit";
    from.library = "Test";
    from.notes = { { 23, "HH Open 2", 46, false }, { 36, "Kick", 36, false }, { 37, "Snare Rim", 38, false },
                   { 61, "Splash FX", kNoGm, false } };
    DrumMap to;
    to.id = "dst-kit";
    to.name = "Dst Kit";
    to.library = "Test";
    to.notes = { { 26, "HH Open", 46, false }, { 36, "Kick R", 36, false }, { 38, "Snare", 38, false } };
    MapLibrary writer (root);
    REQUIRE (writer.saveMap (from).wasOk());
    REQUIRE (writer.saveMap (to).wasOk());
}

juce::MidiMessage at (juce::MidiMessage m, double tick)
{
    m.setTimeStamp (tick);
    return m;
}

// Format 1, 960 ticks per quarter note, two tracks, odd tick positions.
juce::MidiFile makeSource()
{
    juce::MidiFile file;
    file.setTicksPerQuarterNote (960);

    juce::MidiMessageSequence conductor;
    conductor.addEvent (at (juce::MidiMessage::tempoMetaEvent (500000), 0));
    file.addTrack (conductor);

    juce::MidiMessageSequence drums;
    drums.addEvent (at (juce::MidiMessage::noteOn (10, 23, (juce::uint8) 100), 7));
    drums.addEvent (at (juce::MidiMessage::noteOff (10, 23), 131));
    drums.addEvent (at (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 90), 481));
    drums.addEvent (at (juce::MidiMessage::noteOff (10, 36), 520));
    drums.addEvent (at (juce::MidiMessage::noteOn (10, 37, (juce::uint8) 80), 700));
    drums.addEvent (at (juce::MidiMessage::noteOff (10, 37), 710));
    for (int i = 0; i < 3; ++i)
    {
        drums.addEvent (at (juce::MidiMessage::noteOn (10, 61, (juce::uint8) 70), 1000 + 333 * i));
        drums.addEvent (at (juce::MidiMessage::noteOff (10, 61), 1010 + 333 * i));
    }
    file.addTrack (drums);
    return file;
}

bool writeMidi (const juce::File& file, const juce::MidiFile& midi, int type)
{
    juce::FileOutputStream stream (file);
    return stream.openedOk() && midi.writeTo (stream, type);
}

bool readMidi (const juce::File& file, juce::MidiFile& midi, int& type)
{
    juce::FileInputStream stream (file);
    return stream.openedOk() && midi.readFrom (stream, false, &type);
}

struct Fixture
{
    TempRoot root, files;
    std::unique_ptr<SwingersProcessor> processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    juce::File input;

    Fixture()
    {
        prepareProcessor (root.dir);
        processor = std::make_unique<SwingersProcessor> (root.dir);
        processor->selectMaps ("src-kit", "dst-kit");
        PairOverrides overrides;
        overrides.fromId = "src-kit";
        overrides.toId = "dst-kit";
        overrides.drop (37);
        processor->applyOverrides (overrides);

        files.dir.createDirectory();
        input = files.dir.getChildFile ("groove verse.mid");
        REQUIRE (writeMidi (input, makeSource(), 1));

        editor.reset (processor->createEditor());
        asEditor().setFileBarShown (true);
    }

    SwingersEditor& asEditor() { return *dynamic_cast<SwingersEditor*> (editor.get()); }
    FileBar& bar() { return asEditor().fileBarView(); }
    juce::Button& button (const char* id)
    {
        auto* b = dynamic_cast<juce::Button*> (bar().findChildWithID (id));
        REQUIRE (b != nullptr);
        return *b;
    }
};
} // namespace

TEST_CASE ("the file bar is hidden unless forced or running as the standalone app")
{
    juce::ScopedJuceInitialiser_GUI gui;
    TempRoot root;
    SwingersProcessor processor (root.dir);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    auto& swingers = *dynamic_cast<SwingersEditor*> (editor.get());

    CHECK_FALSE (swingers.isFileBarShown());
    CHECK_FALSE (swingers.fileBarView().isVisible());
    CHECK (swingers.getWidth() == 520);
    CHECK (swingers.getHeight() == 140);

    swingers.setFileBarShown (true);
    CHECK (swingers.fileBarView().isVisible());
    CHECK (swingers.getWidth() == 520);
    CHECK (swingers.getHeight() == 330);

    swingers.setFileBarShown (false);
    CHECK (swingers.getHeight() == 140);
}

TEST_CASE ("with the file bar the editor is 520 x 330, 900 x 600 in the patchbay, and comes back")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    CHECK (f.editor->getWidth() == 520);
    CHECK (f.editor->getHeight() == 330);

    f.asEditor().showPatchbay (true);
    CHECK (f.editor->getWidth() == 900);
    CHECK (f.editor->getHeight() == 600);
    CHECK_FALSE (f.bar().isVisible());

    f.asEditor().showPatchbay (false);
    CHECK (f.editor->getWidth() == 520);
    CHECK (f.editor->getHeight() == 330);
    CHECK (f.bar().isVisible());
}

TEST_CASE ("buttons: Convert needs a file, Save needs a converted file; Convert is lit when it is next")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    CHECK_FALSE (f.button ("convertMidi").isEnabled());
    CHECK_FALSE (f.button ("saveMidi").isEnabled());
    CHECK (f.button ("openMidi").isEnabled());
    CHECK_FALSE (f.button ("convertMidi").getToggleState());

    REQUIRE (f.bar().loadFile (f.input));
    CHECK (f.button ("convertMidi").isEnabled());
    CHECK (f.button ("convertMidi").getToggleState()); // lit: the next action
    CHECK_FALSE (f.button ("saveMidi").isEnabled());

    REQUIRE (f.bar().convert());
    CHECK_FALSE (f.button ("convertMidi").getToggleState());
    CHECK (f.button ("saveMidi").isEnabled());

    REQUIRE (f.bar().loadFile (f.input)); // a new file drops the old result
    CHECK_FALSE (f.button ("saveMidi").isEnabled());
}

TEST_CASE ("a file that is not MIDI is reported and nothing is enabled")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    const auto junk = f.files.dir.getChildFile ("junk.mid");
    REQUIRE (junk.replaceWithText ("not a midi file"));

    CHECK_FALSE (f.bar().loadFile (junk));
    CHECK (f.bar().reportLines() == juce::StringArray { "Cannot read the MIDI file" });
    CHECK_FALSE (f.bar().hasFile());
    CHECK_FALSE (f.button ("convertMidi").isEnabled());
}

TEST_CASE ("a failed open keeps the file that was loaded and shows the error")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    REQUIRE (f.bar().loadFile (f.input));
    REQUIRE (f.bar().convert());

    const auto junk = f.files.dir.getChildFile ("junk.mid");
    REQUIRE (junk.replaceWithText ("not a midi file"));
    CHECK_FALSE (f.bar().loadFile (junk));
    CHECK (f.bar().hasFile());
    CHECK (f.bar().sourceFile() == f.input);
    CHECK (f.bar().hasConverted());
    CHECK (f.button ("saveMidi").isEnabled());
    CHECK (f.bar().reportLines().strings.getLast() == "Cannot read the MIDI file");
    CHECK (f.bar().reportLines()[0].startsWith ("Tracks: 1"));
}

TEST_CASE ("convert reports tracks, notes and the dropped notes with their names")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    REQUIRE (f.bar().loadFile (f.input));
    REQUIRE (f.bar().convert());

    const auto dot = juce::String::fromUTF8 (" \xc2\xb7 ");
    const auto times = juce::String::fromUTF8 ("\xc3\x97");
    const auto lines = f.bar().reportLines();
    REQUIRE (lines.size() == 3);
    CHECK (lines[0] == "Tracks: 1" + dot + "notes converted: 2"); // 23 -> 26 and 36
    CHECK (lines[1] == "Dropped (no match): 61 Splash FX " + times + "3");
    CHECK (lines[2] == "Dropped (disconnected): 37 Snare Rim " + times + "1");
}

TEST_CASE ("nothing dropped shows one line, and a note the map does not name shows just the number")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    // Drop nothing: the pair has no exceptions.
    PairOverrides none;
    none.fromId = "src-kit";
    none.toId = "dst-kit";
    f.processor->applyOverrides (none);

    juce::MidiFile midi;
    midi.setTicksPerQuarterNote (480);
    juce::MidiMessageSequence track;
    track.addEvent (at (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 100), 0));
    track.addEvent (at (juce::MidiMessage::noteOff (10, 36), 10));
    midi.addTrack (track);
    const auto simple = f.files.dir.getChildFile ("simple.mid");
    REQUIRE (writeMidi (simple, midi, 1));

    REQUIRE (f.bar().loadFile (simple));
    REQUIRE (f.bar().convert());
    const auto lines = f.bar().reportLines();
    REQUIRE (lines.size() == 2);
    CHECK (lines[1] == "Dropped: none");

    juce::MidiMessageSequence unnamed;
    unnamed.addEvent (at (juce::MidiMessage::noteOn (10, 100, (juce::uint8) 100), 0)); // not in the From map
    unnamed.addEvent (at (juce::MidiMessage::noteOff (10, 100), 10));
    juce::MidiFile other;
    other.setTicksPerQuarterNote (480);
    other.addTrack (unnamed);
    const auto unnamedFile = f.files.dir.getChildFile ("unnamed.mid");
    REQUIRE (writeMidi (unnamedFile, other, 1));
    REQUIRE (f.bar().loadFile (unnamedFile));
    REQUIRE (f.bar().convert());
    CHECK (f.bar().reportLines()[1] == "Dropped (no match): 100 " + juce::String::fromUTF8 ("\xc3\x97") + "1");
}

TEST_CASE ("a file with no notes on the selected channel shows the converter's error")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    f.processor->setChannel (3); // the notes are on channel 10
    REQUIRE (f.bar().loadFile (f.input));
    CHECK_FALSE (f.bar().convert());
    CHECK (f.bar().reportLines() == juce::StringArray { "No notes on the selected channel" });
    CHECK_FALSE (f.bar().hasConverted());
}

TEST_CASE ("the suggested name is <name>-<To map>.mid next to the original")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    REQUIRE (f.bar().loadFile (f.input));
    REQUIRE (f.bar().convert());
    CHECK (f.bar().suggestedOutput() == f.files.dir.getChildFile ("groove verse-Dst-Kit.mid"));
}

TEST_CASE ("save writes the converted file with the same type, resolution and ticks")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    REQUIRE (f.bar().loadFile (f.input));
    REQUIRE (f.bar().convert());

    const auto out = f.bar().suggestedOutput();
    REQUIRE (f.bar().saveTo (out));
    CHECK (f.bar().reportLines().strings.getLast() == "Saved: " + out.getFileName());

    juce::MidiFile in, saved;
    int inType = -1, outType = -1;
    REQUIRE (readMidi (f.input, in, inType));
    REQUIRE (readMidi (out, saved, outType));
    CHECK (outType == inType);
    CHECK (saved.getTimeFormat() == 960);
    REQUIRE (saved.getNumTracks() == 2);

    // Drums track: 23 -> 26, 36 stays, 37 (dropped by the user) and 61 (no match) are gone.
    const auto* converted = saved.getTrack (1);
    std::vector<juce::MidiMessage> notes; // the writer adds an end-of-track meta event
    for (int i = 0; i < converted->getNumEvents(); ++i)
        if (const auto& m = converted->getEventPointer (i)->message; m.isNoteOnOrOff())
            notes.push_back (m);
    REQUIRE (notes.size() == 4);
    const int expectedNotes[] = { 26, 26, 36, 36 };
    const int expectedTicks[] = { 7, 131, 481, 520 };
    for (size_t i = 0; i < 4; ++i)
    {
        const auto& m = notes[i];
        CHECK (m.getNoteNumber() == expectedNotes[i]);
        CHECK ((int) m.getTimeStamp() == expectedTicks[i]);
        CHECK (m.getChannel() == 10);
    }

    // The conductor track is untouched.
    REQUIRE (saved.getTrack (0)->getNumEvents() == in.getTrack (0)->getNumEvents());
    CHECK (saved.getTrack (0)->getEventPointer (0)->message.isTempoMetaEvent());
}

TEST_CASE ("save refuses to overwrite the source file and leaves it untouched")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    REQUIRE (f.bar().loadFile (f.input));
    REQUIRE (f.bar().convert());

    juce::MemoryBlock before, after;
    REQUIRE (f.input.loadFileAsData (before));
    CHECK_FALSE (f.bar().saveTo (f.input));
    CHECK (f.bar().reportLines().strings.getLast() == "The output file must be different from the input file");
    REQUIRE (f.input.loadFileAsData (after));
    CHECK (after == before);
}

TEST_CASE ("converting again after a map change uses the new table")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    REQUIRE (f.bar().loadFile (f.input));
    REQUIRE (f.bar().convert());
    CHECK (f.bar().reportLines()[2].startsWith ("Dropped (disconnected): 37"));

    PairOverrides none;
    none.fromId = "src-kit";
    none.toId = "dst-kit";
    none.connect (37, 38);
    f.processor->applyOverrides (none);
    REQUIRE (f.bar().convert());
    const auto lines = f.bar().reportLines();
    REQUIRE (lines.size() == 3);
    CHECK (lines[0].endsWith ("notes converted: 3"));
    CHECK (lines[2] == "Dropped (disconnected): none");
}

TEST_CASE ("the file bar draws without trouble in every state")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    const auto snapshot = [&] { return f.editor->createComponentSnapshot (f.editor->getLocalBounds(), true, 1.0f); };
    CHECK (snapshot().isValid());
    REQUIRE (f.bar().loadFile (f.input));
    REQUIRE (f.bar().convert());
    REQUIRE (f.bar().saveTo (f.bar().suggestedOutput()));
    CHECK (snapshot().isValid());
}

TEST_CASE ("a conversion is dropped when the maps, the overrides or the channel change")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    const auto convertedFile = [&] {
        REQUIRE (f.bar().loadFile (f.input));
        REQUIRE (f.bar().convert());
        REQUIRE (f.bar().hasConverted());
        REQUIRE (f.button ("saveMidi").isEnabled());
    };
    const auto requireInvalidated = [&] {
        CHECK_FALSE (f.bar().hasConverted());
        CHECK_FALSE (f.button ("saveMidi").isEnabled());
        CHECK (f.bar().hasFile());
        CHECK (f.button ("convertMidi").isEnabled());
        CHECK (f.button ("convertMidi").getToggleState()); // lit again
        CHECK (f.bar().reportLines() == juce::StringArray { "Maps changed: convert again" });
    };

    SECTION ("overrides")
    {
        convertedFile();
        PairOverrides overrides;
        overrides.fromId = "src-kit";
        overrides.toId = "dst-kit";
        overrides.connect (37, 38);
        f.processor->applyOverrides (overrides);
        requireInvalidated();
    }

    SECTION ("selected maps")
    {
        convertedFile();
        f.processor->selectMaps ("dst-kit", "src-kit");
        requireInvalidated();
    }

    SECTION ("channel")
    {
        convertedFile();
        f.processor->setChannel (10);
        requireInvalidated();
    }

    SECTION ("the current To map saved from the map editor")
    {
        convertedFile();
        auto& swingers = f.asEditor();
        swingers.showMapEditor (true);
        auto& view = swingers.mapEditorView();
        view.setNoteName (26, "HH Open Edited");
        REQUIRE (view.save());
        requireInvalidated();
    }

    SECTION ("a save dialog that returns afterwards says so instead of doing nothing")
    {
        convertedFile();
        f.processor->setChannel (10);
        const auto out = f.files.dir.getChildFile ("late.mid");
        CHECK_FALSE (f.bar().saveTo (out));
        CHECK_FALSE (out.existsAsFile());
        CHECK (f.bar().reportLines().strings.getLast() == "Nothing to save: convert again");
    }

    SECTION ("a change that does not affect the result keeps it")
    {
        convertedFile();
        f.processor->wearSeed(); // no maps change
        f.processor->selectMaps ("src-kit", "dst-kit"); // same pair, same maps
        CHECK (f.bar().hasConverted());
        CHECK (f.button ("saveMidi").isEnabled());
    }
}

TEST_CASE ("save adds .mid when the chosen name has no extension")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    REQUIRE (f.bar().loadFile (f.input));
    REQUIRE (f.bar().convert());
    const auto chosen = f.files.dir.getChildFile ("renamed");
    REQUIRE (f.bar().saveTo (chosen));
    CHECK (f.files.dir.getChildFile ("renamed.mid").existsAsFile());
    CHECK_FALSE (chosen.existsAsFile());
    CHECK (f.bar().reportLines().strings.getLast() == "Saved: renamed.mid");
}

TEST_CASE ("save refuses the source reached through a symlinked folder")
{
    juce::ScopedJuceInitialiser_GUI gui;
    Fixture f;
    const auto link = f.files.dir.getParentDirectory().getChildFile ("swingers-link-" + juce::Uuid().toString());
    if (! f.files.dir.createSymbolicLink (link, true))
        SKIP ("symbolic links are not available here");

    REQUIRE (f.bar().loadFile (f.input));
    REQUIRE (f.bar().convert());
    juce::MemoryBlock before, after;
    REQUIRE (f.input.loadFileAsData (before));
    CHECK_FALSE (f.bar().saveTo (link.getChildFile (f.input.getFileName())));
    CHECK (f.bar().reportLines().strings.getLast() == "The output file must be different from the input file");
    REQUIRE (f.input.loadFileAsData (after));
    CHECK (after == before);
    link.deleteFile();
}
