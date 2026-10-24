#include <catch2/catch_test_macros.hpp>

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "PluginState.h"

using namespace swingers;

namespace
{
struct TempRoot
{
    juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("swingers-state-" + juce::Uuid().toString());
    ~TempRoot() { dir.deleteRecursively(); }
};

DrumMap kit (const juce::String& id, const juce::String& name, int note, int gm)
{
    DrumMap map;
    map.id = id;
    map.name = name;
    map.library = "Test";
    map.notes = { { note, "Voice", gm, true } };
    return map;
}

juce::String jsonOf (const juce::var& v) { return juce::JSON::toString (v, true); }

int firstNoteOn (const juce::MidiBuffer& midi)
{
    for (const auto meta : midi)
        if (meta.getMessage().isNoteOn())
            return meta.getMessage().getNoteNumber();
    return -1;
}
} // namespace

TEST_CASE ("state round trip")
{
    PluginState state;
    state.fromId = "my-kit";
    state.toId = "other-kit";
    state.channel = 10;
    state.fromCopy = kit ("my-kit", "My Kit", 23, 46);
    state.toCopy = kit ("other-kit", "Other", 40, 38);
    PairOverrides overrides;
    overrides.fromId = "my-kit";
    overrides.toId = "other-kit";
    overrides.connect (23, 40);
    overrides.drop (24);
    state.overridesCopy = overrides;

    const auto block = serialize (state);
    const auto back = deserialize (block.getData(), block.getSize());

    REQUIRE (back.has_value());
    CHECK (back->fromId == state.fromId);
    CHECK (back->toId == state.toId);
    CHECK (back->channel == 10);
    REQUIRE (back->fromCopy.has_value());
    REQUIRE (back->toCopy.has_value());
    REQUIRE (back->overridesCopy.has_value());
    CHECK (jsonOf (toJson (*back->fromCopy)) == jsonOf (toJson (*state.fromCopy)));
    CHECK (jsonOf (toJson (*back->toCopy)) == jsonOf (toJson (*state.toCopy)));
    CHECK (jsonOf (toJson (*back->overridesCopy)) == jsonOf (toJson (overrides)));
    CHECK (back->overridesCopy->direct.at (23) == 40);
    CHECK (back->overridesCopy->dropped.count (24) == 1);
}

TEST_CASE ("deserialize rejects other formats and tolerates broken copies")
{
    const juce::String wrongFormat = R"({"format":"something","version":1})";
    CHECK_FALSE (deserialize (wrongFormat.toRawUTF8(), wrongFormat.getNumBytesAsUTF8()).has_value());

    const juce::String wrongVersion = R"({"format":"swingers-state","version":2})";
    CHECK_FALSE (deserialize (wrongVersion.toRawUTF8(), wrongVersion.getNumBytesAsUTF8()).has_value());

    const juce::String garbage = "not json at all";
    CHECK_FALSE (deserialize (garbage.toRawUTF8(), garbage.getNumBytesAsUTF8()).has_value());
    CHECK_FALSE (deserialize (nullptr, 0).has_value());

    const juce::String broken =
        R"({"format":"swingers-state","version":1,"from":"a","to":"b","channel":99,"fromMap":{"x":1},"toMap":5,"overrides":"no"})";
    const auto state = deserialize (broken.toRawUTF8(), broken.getNumBytesAsUTF8());
    REQUIRE (state.has_value());
    CHECK (state->fromId == "a");
    CHECK (state->toId == "b");
    CHECK (state->channel == 16);
    CHECK_FALSE (state->fromCopy.has_value());
    CHECK_FALSE (state->toCopy.has_value());
    CHECK_FALSE (state->overridesCopy.has_value());
}

TEST_CASE ("missing map falls back to the saved copy")
{
    TempRoot tmp;
    MapLibrary library (tmp.dir);

    PluginState state;
    state.fromId = "my-kit";
    state.fromCopy = kit ("my-kit", "My Kit", 23, 46);

    const auto resolved = resolve (state, library);

    CHECK (resolved.from.id == "my-kit");
    CHECK (resolved.to.id == kGeneralMidiId);
    REQUIRE (resolved.warnings.size() == 1);
    CHECK (resolved.warnings[0] == "Map 'My Kit' not found on this computer: using the copy saved in the project");
}

TEST_CASE ("missing map without a copy falls back to General MIDI")
{
    TempRoot tmp;
    MapLibrary library (tmp.dir);

    PluginState state;
    state.fromId = "gone";

    const auto resolved = resolve (state, library);
    CHECK (resolved.from.id == kGeneralMidiId);
    CHECK (resolved.warnings.isEmpty());
}

TEST_CASE ("library overrides win over the copy, the copy is used when there is no pair file")
{
    TempRoot tmp;
    MapLibrary library (tmp.dir);

    PluginState state;
    state.fromId = kGeneralMidiId;
    state.toId = kGeneralMidiId;
    PairOverrides copy;
    copy.fromId = kGeneralMidiId;
    copy.toId = kGeneralMidiId;
    copy.connect (36, 38);
    state.overridesCopy = copy;

    CHECK (resolve (state, library).overrides.direct.at (36) == 38);

    PairOverrides saved = copy;
    saved.connect (36, 40);
    REQUIRE (library.saveOverrides (saved).wasOk());
    CHECK (resolve (state, library).overrides.direct.at (36) == 40);
}

TEST_CASE ("processor remaps in processBlock")
{
    TempRoot tmp;
    {
        MapLibrary writer (tmp.dir);
        REQUIRE (writer.saveMap (kit ("my-kit", "My Kit", 23, 46)).wasOk());
    }

    SwingersProcessor processor (tmp.dir);
    int changes = 0;
    processor.onStateChanged = [&] { ++changes; };
    processor.selectMaps ("my-kit", "general-midi");
    CHECK (changes == 1);
    CHECK (processor.current().from.id == "my-kit");

    juce::AudioBuffer<float> audio (1, 32);
    audio.setSample (0, 0, 1.0f);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 23, (juce::uint8) 100), 0);
    processor.processBlock (audio, midi);

    CHECK (firstNoteOn (midi) == 46);
    CHECK (audio.getMagnitude (0, 0, 32) < 0.0001f);
}

TEST_CASE ("processor channel filter and overrides rebuild the table")
{
    TempRoot tmp;
    SwingersProcessor processor (tmp.dir);
    int changes = 0;
    processor.onStateChanged = [&] { ++changes; };

    processor.setChannel (10);
    CHECK (changes == 1);

    PairOverrides overrides;
    overrides.fromId = kGeneralMidiId;
    overrides.toId = kGeneralMidiId;
    overrides.connect (36, 38);
    processor.applyOverrides (overrides);
    CHECK (changes == 2);
    CHECK (processor.library().overrides (kGeneralMidiId, kGeneralMidiId).direct.at (36) == 38);

    juce::AudioBuffer<float> audio (1, 32);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 100), 0);
    midi.addEvent (juce::MidiMessage::noteOn (2, 36, (juce::uint8) 100), 1);
    processor.processBlock (audio, midi);

    std::vector<int> notes;
    for (const auto meta : midi)
        notes.push_back (meta.getMessage().getNoteNumber());
    REQUIRE (notes.size() == 2);
    CHECK (notes[0] == 38); // channel 10 remapped
    CHECK (notes[1] == 36); // other channel untouched
}

TEST_CASE ("processor state round trip carries copies to another computer")
{
    TempRoot tmpA, tmpB;
    {
        MapLibrary writer (tmpA.dir);
        REQUIRE (writer.saveMap (kit ("my-kit", "My Kit", 23, 46)).wasOk());
    }

    SwingersProcessor a (tmpA.dir);
    a.selectMaps ("my-kit", "general-midi");
    a.setChannel (3);
    juce::MemoryBlock block;
    a.getStateInformation (block);

    SwingersProcessor b (tmpB.dir); // no my-kit here
    int changes = 0;
    b.onStateChanged = [&] { ++changes; };
    b.setStateInformation (block.getData(), (int) block.getSize());

    CHECK (changes == 1);
    CHECK (b.current().from.id == "my-kit");
    CHECK (b.current().warnings.size() == 1);

    juce::AudioBuffer<float> audio (1, 32);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (3, 23, (juce::uint8) 100), 0);
    b.processBlock (audio, midi);
    CHECK (firstNoteOn (midi) == 46);

    const juce::String bad = "garbage";
    b.setStateInformation (bad.toRawUTF8(), bad.getNumBytesAsUTF8());
    CHECK (b.current().from.id == "my-kit"); // untouched
    CHECK (changes == 1);
}
