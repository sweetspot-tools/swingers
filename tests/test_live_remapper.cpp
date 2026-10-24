#include <catch2/catch_test_macros.hpp>

#include "LiveRemapper.h"

#include <vector>

using namespace swingers;

namespace
{
struct Event
{
    std::vector<juce::uint8> bytes;
    int position;
};

std::vector<Event> eventsOf (const juce::MidiBuffer& buffer)
{
    std::vector<Event> out;

    for (const auto meta : buffer)
        out.push_back ({ std::vector<juce::uint8> (meta.data, meta.data + meta.numBytes), meta.samplePosition });

    return out;
}
} // namespace

TEST_CASE ("live remapper counts hits and removes dropped events")
{
    auto table = ConversionTable::identity();
    table.entries[61].route = Route::DroppedByUser;

    LiveRemapper remapper;
    remapper.setTable (table);

    juce::MidiBuffer buffer;
    buffer.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 3);
    buffer.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 90), 10);
    buffer.addEvent (juce::MidiMessage::noteOn (1, 61, (juce::uint8) 80), 20);
    buffer.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 70), 30);

    remapper.process (buffer);

    CHECK (remapper.activity() == 3);
    CHECK (remapper.hits (36) == 3);
    CHECK (remapper.hits (61) == 0);
    CHECK (remapper.hits (-1) == 0);
    CHECK (remapper.hits (128) == 0);

    const auto out = eventsOf (buffer);
    REQUIRE (out.size() == 3);
    CHECK (out[0].position == 3);
    CHECK (out[1].position == 10);
    CHECK (out[2].position == 30);

    for (const auto& e : out)
        CHECK (e.bytes[1] == 36);
}

TEST_CASE ("live remapper remaps notes and keeps note offs matched across a table change")
{
    auto first = ConversionTable::identity();
    first.entries[23].target = 26;
    first.entries[23].route = Route::Override;

    auto second = ConversionTable::identity();
    second.entries[23].target = 30;
    second.entries[23].route = Route::Override;

    LiveRemapper remapper;
    remapper.setTable (first);

    juce::MidiBuffer block1;
    block1.addEvent (juce::MidiMessage::noteOn (1, 23, (juce::uint8) 100), 0);
    remapper.process (block1);

    remapper.setTable (second);

    juce::MidiBuffer block2;
    block2.addEvent (juce::MidiMessage::noteOff (1, 23, (juce::uint8) 40), 5);
    remapper.process (block2);

    const auto out1 = eventsOf (block1);
    const auto out2 = eventsOf (block2);
    REQUIRE (out1.size() == 1);
    REQUIRE (out2.size() == 1);
    CHECK (out1[0].bytes[1] == 26);
    CHECK (out2[0].bytes[0] == 0x80);
    CHECK (out2[0].bytes[1] == 26);
    CHECK (out2[0].bytes[2] == 40);

    // A note-off is not a hit.
    CHECK (remapper.activity() == 1);
    CHECK (remapper.hits (23) == 1);
}

TEST_CASE ("live remapper passes other messages through unchanged and keeps their order")
{
    LiveRemapper remapper;
    remapper.setChannel (10);

    const juce::uint8 sysexData[] = { 0xF0, 0x7E, 0x00, 0x09, 0x01, 0xF7 };

    juce::MidiBuffer buffer;
    buffer.addEvent (juce::MidiMessage::controllerEvent (10, 64, 127), 1);
    buffer.addEvent (sysexData, (int) sizeof (sysexData), 1);
    buffer.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 1); // wrong channel
    buffer.addEvent (juce::MidiMessage::pitchWheel (10, 9000), 2);

    const auto before = eventsOf (buffer);
    remapper.process (buffer);
    const auto after = eventsOf (buffer);

    REQUIRE (after.size() == before.size());

    for (size_t i = 0; i < before.size(); ++i)
    {
        CHECK (after[i].bytes == before[i].bytes);
        CHECK (after[i].position == before[i].position);
    }

    CHECK (remapper.activity() == 0);
}

TEST_CASE ("live remapper handles a block larger than the scratch capacity")
{
    LiveRemapper remapper;

    juce::MidiBuffer buffer;

    for (int i = 0; i < 1000; ++i)
        buffer.addEvent (juce::MidiMessage::noteOn (1, i % 128, (juce::uint8) 100), i);

    remapper.process (buffer);

    CHECK (eventsOf (buffer).size() == 1000);
    CHECK (remapper.activity() == 1000);
}
