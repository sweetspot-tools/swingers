#include <catch2/catch_test_macros.hpp>

#include "NoteRouter.h"

#include <cstring>
#include <initializer_list>
#include <utility>

using namespace swingers;
using Outcome = NoteRouter::Outcome;

namespace
{
ConversionTable tableWith (std::initializer_list<std::pair<int, int>> mappings)
{
    auto table = ConversionTable::identity();

    for (const auto& [from, to] : mappings)
    {
        table.entries[(size_t) from].target = to;
        table.entries[(size_t) from].route = Route::Override;
    }

    return table;
}

void drop (ConversionTable& table, int note)
{
    table.entries[(size_t) note].route = Route::DroppedByUser;
}

juce::MidiMessage on (int note, int velocity = 100, int channel = 1) { return juce::MidiMessage::noteOn (channel, note, (juce::uint8) velocity); }
juce::MidiMessage off (int note, int channel = 1) { return juce::MidiMessage::noteOff (channel, note, (juce::uint8) 64); }
} // namespace

TEST_CASE ("remaps note on and the matching off")
{
    NoteRouter router;
    const auto table = tableWith ({ { 23, 26 } });

    auto noteOn = on (23);
    auto noteOff = off (23);

    CHECK (router.route (noteOn, table, 0) == Outcome::Remapped);
    CHECK (router.route (noteOff, table, 0) == Outcome::Remapped);

    CHECK (noteOn.isNoteOn());
    CHECK (noteOn.getNoteNumber() == 26);
    CHECK (noteOn.getVelocity() == 100);
    CHECK (noteOff.isNoteOff());
    CHECK (noteOff.getNoteNumber() == 26);
    CHECK (noteOff.getRawData()[2] == 64);
}

TEST_CASE ("velocity zero note on acts as note off")
{
    NoteRouter router;
    const auto table = tableWith ({ { 23, 26 } });

    auto first = on (23, 100);
    auto second = on (23, 0);

    router.route (first, table, 0);
    CHECK (router.route (second, table, 0) == Outcome::Remapped);
    CHECK (second.getNoteNumber() == 26);
    CHECK (second.getRawData()[2] == 0);

    // The held entry was cleared: a later stray off falls back to the table, not to a stale entry.
    const auto otherTable = tableWith ({ { 23, 30 } });
    auto stray = off (23);
    router.route (stray, otherTable, 0);
    CHECK (stray.getNoteNumber() == 30);
}

TEST_CASE ("note off follows the note on after a table change")
{
    NoteRouter router;
    auto noteOn = on (23);
    auto noteOff = off (23);

    router.route (noteOn, tableWith ({ { 23, 26 } }), 0);
    CHECK (noteOn.getNoteNumber() == 26);

    CHECK (router.route (noteOff, tableWith ({ { 23, 30 } }), 0) == Outcome::Remapped);
    CHECK (noteOff.getNoteNumber() == 26);
}

TEST_CASE ("dropped notes drop their off too")
{
    NoteRouter router;
    auto table = ConversionTable::identity();
    drop (table, 61);

    auto noteOn = on (61);
    auto noteOff = off (61);

    CHECK (router.route (noteOn, table, 0) == Outcome::Dropped);
    CHECK (router.route (noteOff, table, 0) == Outcome::Dropped);
}

TEST_CASE ("a note dropped at note on stays dropped for its off even if the table starts routing it")
{
    NoteRouter router;
    auto dropping = ConversionTable::identity();
    drop (dropping, 61);

    auto noteOn = on (61);
    auto noteOff = off (61);

    CHECK (router.route (noteOn, dropping, 0) == Outcome::Dropped);
    CHECK (router.route (noteOff, ConversionTable::identity(), 0) == Outcome::Dropped);

    // And the entry is cleared afterwards.
    auto again = off (61);
    CHECK (router.route (again, ConversionTable::identity(), 0) == Outcome::Remapped);
}

TEST_CASE ("two sources on one target leave nothing hanging")
{
    NoteRouter router;
    const auto table = tableWith ({ { 46, 26 }, { 23, 26 } });

    auto on46 = on (46);
    auto on23 = on (23);
    auto off46 = off (46);
    auto off23 = off (23);

    CHECK (router.route (on46, table, 0) == Outcome::Remapped);
    CHECK (router.route (on23, table, 0) == Outcome::Remapped);
    CHECK (router.route (off46, table, 0) == Outcome::Remapped);
    CHECK (router.route (off23, table, 0) == Outcome::Remapped);

    CHECK (on46.isNoteOn());
    CHECK (on23.isNoteOn());
    CHECK (off46.isNoteOff());
    CHECK (off23.isNoteOff());
    CHECK (on46.getNoteNumber() == 26);
    CHECK (on23.getNoteNumber() == 26);
    CHECK (off46.getNoteNumber() == 26);
    CHECK (off23.getNoteNumber() == 26);
}

TEST_CASE ("other channels and messages pass untouched")
{
    NoteRouter router;
    const auto table = tableWith ({ { 23, 26 } });

    auto wrongChannel = on (23, 100, 1);
    const auto wrongChannelBefore = wrongChannel;
    auto controller = juce::MidiMessage::controllerEvent (10, 64, 127);
    const auto controllerBefore = controller;
    auto bend = juce::MidiMessage::pitchWheel (10, 12345);
    const auto bendBefore = bend;

    CHECK (router.route (wrongChannel, table, 10) == Outcome::Pass);
    CHECK (router.route (controller, table, 10) == Outcome::Pass);
    CHECK (router.route (bend, table, 10) == Outcome::Pass);

    CHECK (wrongChannel.getDescription() == wrongChannelBefore.getDescription());
    CHECK (std::memcmp (wrongChannel.getRawData(), wrongChannelBefore.getRawData(), 3) == 0);
    CHECK (std::memcmp (controller.getRawData(), controllerBefore.getRawData(), 3) == 0);
    CHECK (std::memcmp (bend.getRawData(), bendBefore.getRawData(), 3) == 0);

    auto rightChannel = on (23, 100, 10);
    CHECK (router.route (rightChannel, table, 10) == Outcome::Remapped);
    CHECK (rightChannel.getNoteNumber() == 26);
}

TEST_CASE ("the same note on different channels is tracked separately")
{
    NoteRouter router;
    auto noteOn1 = on (23, 100, 1);
    auto noteOn2 = on (23, 100, 2);

    router.route (noteOn1, tableWith ({ { 23, 26 } }), 0);
    router.route (noteOn2, tableWith ({ { 23, 30 } }), 0);

    auto off1 = off (23, 1);
    auto off2 = off (23, 2);
    const auto changed = tableWith ({ { 23, 40 } });
    router.route (off1, changed, 0);
    router.route (off2, changed, 0);

    CHECK (off1.getNoteNumber() == 26);
    CHECK (off2.getNoteNumber() == 30);
    CHECK (off1.getChannel() == 1);
    CHECK (off2.getChannel() == 2);
}

TEST_CASE ("a second note on for a held source overwrites the held target")
{
    NoteRouter router;
    auto first = on (23);
    auto second = on (23);
    auto noteOff = off (23);

    router.route (first, tableWith ({ { 23, 26 } }), 0);
    router.route (second, tableWith ({ { 23, 30 } }), 0);
    router.route (noteOff, tableWith ({ { 23, 40 } }), 0);

    CHECK (noteOff.getNoteNumber() == 30);
}

TEST_CASE ("polyphonic aftertouch follows the held note, else the table")
{
    NoteRouter router;
    auto noteOn = on (23);
    router.route (noteOn, tableWith ({ { 23, 26 } }), 0);

    auto heldPressure = juce::MidiMessage::aftertouchChange (1, 23, 77);
    CHECK (router.route (heldPressure, tableWith ({ { 23, 30 } }), 0) == Outcome::Remapped);
    CHECK (heldPressure.getNoteNumber() == 26);
    CHECK (heldPressure.getAfterTouchValue() == 77);

    // Aftertouch does not release the note.
    auto noteOff = off (23);
    router.route (noteOff, tableWith ({ { 23, 30 } }), 0);
    CHECK (noteOff.getNoteNumber() == 26);

    auto freePressure = juce::MidiMessage::aftertouchChange (1, 23, 10);
    CHECK (router.route (freePressure, tableWith ({ { 23, 30 } }), 0) == Outcome::Remapped);
    CHECK (freePressure.getNoteNumber() == 30);
}

TEST_CASE ("unroutable entries are dropped, including out-of-range targets")
{
    NoteRouter router;
    auto table = ConversionTable::identity();
    table.entries[10].route = Route::NoMatch;
    table.entries[11].target = 200;
    table.entries[11].route = Route::Override;
    table.entries[12].target = -5;
    table.entries[12].route = Route::Override;

    for (int note : { 10, 11, 12 })
    {
        auto noteOn = on (note);
        auto noteOff = off (note);
        CHECK (router.route (noteOn, table, 0) == Outcome::Dropped);
        CHECK (router.route (noteOff, table, 0) == Outcome::Dropped);
    }
}

TEST_CASE ("a note off for a held note passes the channel filter even if the filter changed")
{
    NoteRouter router;
    const auto table = tableWith ({ { 23, 26 } });

    auto noteOn = on (23, 100, 1);
    router.route (noteOn, table, 0);

    auto noteOff = off (23, 1);
    CHECK (router.route (noteOff, table, 10) == Outcome::Remapped);
    CHECK (noteOff.getNoteNumber() == 26);
}

TEST_CASE ("reset forgets held notes")
{
    NoteRouter router;
    auto noteOn = on (23);
    router.route (noteOn, tableWith ({ { 23, 26 } }), 0);
    router.reset();

    auto noteOff = off (23);
    router.route (noteOff, tableWith ({ { 23, 30 } }), 0);
    CHECK (noteOff.getNoteNumber() == 30);
}

TEST_CASE ("a note on passed by the channel filter keeps its off unchanged when the filter widens")
{
    NoteRouter router;
    const auto table = tableWith ({ { 23, 26 } });

    auto noteOn = on (23, 100, 1);
    CHECK (router.route (noteOn, table, 10) == Outcome::Pass);
    CHECK (noteOn.getNoteNumber() == 23);

    auto noteOff = off (23, 1);
    router.route (noteOff, table, 0);
    CHECK (noteOff.getNoteNumber() == 23);

    // The entry is cleared: the next off goes through the table again.
    auto later = off (23, 1);
    router.route (later, table, 0);
    CHECK (later.getNoteNumber() == 26);
}

TEST_CASE ("an off on an excluded channel for a self-held note passes untouched")
{
    NoteRouter router;
    const auto table = tableWith ({ { 23, 26 } });

    auto noteOn = on (23, 100, 1);
    router.route (noteOn, table, 10);

    auto noteOff = off (23, 1);
    CHECK (router.route (noteOff, table, 10) == Outcome::Pass);
    CHECK (noteOff.getNoteNumber() == 23);
}
