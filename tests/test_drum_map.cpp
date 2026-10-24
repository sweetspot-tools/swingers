#include <catch2/catch_test_macros.hpp>

#include <juce_core/juce_core.h>

#include "BinaryData.h"
#include "DrumMap.h"

using namespace swingers;

namespace
{
juce::var parse (const juce::String& text)
{
    juce::var v;
    REQUIRE (juce::JSON::parse (text, v).wasOk());
    return v;
}

bool sameNote (const MapNote& a, const MapNote& b)
{
    return a.note == b.note && a.name == b.name && a.gm == b.gm && a.preferred == b.preferred;
}
}

TEST_CASE ("round trip keeps every field")
{
    DrumMap map;
    map.id = "my-map";
    map.name = "My Map";
    map.library = "My Library";
    map.notes = { { 36, "Kick", 36, false },
                  { 38, "Snare", 38, true },
                  { 60, "Weird Thing", kNoGm, false } };

    juce::StringArray problems;
    const auto back = drumMapFromJson (toJson (map), problems);

    REQUIRE (back.has_value());
    CHECK (problems.isEmpty());
    CHECK (back->id == map.id);
    CHECK (back->name == map.name);
    CHECK (back->library == map.library);
    REQUIRE (back->notes.size() == map.notes.size());
    for (size_t i = 0; i < map.notes.size(); ++i)
        CHECK (sameNote (back->notes[i], map.notes[i]));
}

TEST_CASE ("rejects notes out of range and duplicates")
{
    const auto json = parse (R"({"format":"swingers-map","version":1,"id":"x","name":"X","library":"L","notes":[
        {"note":40,"name":"A","gm":40},
        {"note":128,"name":"Too High"},
        {"note":36,"name":"First","gm":36},
        {"note":36,"name":"Second","gm":36}]})");

    juce::StringArray problems;
    const auto map = drumMapFromJson (json, problems);

    REQUIRE (map.has_value());
    CHECK (problems.size() == 2);
    REQUIRE (map->notes.size() == 2);
    CHECK (map->notes[0].note == 36);
    CHECK (map->notes[0].name == "First");
    CHECK (map->notes[1].note == 40);
}

TEST_CASE ("rejects wrong format")
{
    const auto json = parse (R"({"format":"other","version":1,"id":"x","name":"X","library":"L","notes":[]})");
    juce::StringArray problems;
    CHECK_FALSE (drumMapFromJson (json, problems).has_value());
}

TEST_CASE ("setPreferred clears the others on the same voice")
{
    DrumMap map;
    map.notes = { { 38, "Snare", 38, false },
                  { 40, "Snare 2", 38, false },
                  { 36, "Kick", 36, true } };

    map.setPreferred (38);
    CHECK (map.find (38)->preferred);
    CHECK_FALSE (map.find (40)->preferred);

    map.setPreferred (40);
    CHECK_FALSE (map.find (38)->preferred);
    CHECK (map.find (40)->preferred);
    CHECK (map.find (36)->preferred);
}

TEST_CASE ("find and gmVoiceOf")
{
    DrumMap map;
    map.notes = { { 36, "Kick", 36, false }, { 60, "Odd", kNoGm, false } };

    CHECK (map.find (36) != nullptr);
    CHECK (map.find (37) == nullptr);
    CHECK (map.gmVoiceOf (36) == 36);
    CHECK (map.gmVoiceOf (60) == kNoGm);
    CHECK (map.gmVoiceOf (99) == kNoGm);
}

TEST_CASE ("bundled GM map")
{
    juce::String resourceName;
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        if (juce::String (BinaryData::originalFilenames[i]) == "general-midi.swmap.json")
            resourceName = BinaryData::namedResourceList[i];

    REQUIRE (resourceName.isNotEmpty());

    int size = 0;
    const auto* data = BinaryData::getNamedResource (resourceName.toRawUTF8(), size);
    REQUIRE (data != nullptr);

    juce::StringArray problems;
    const auto map = drumMapFromJson (parse (juce::String::fromUTF8 (data, size)), problems);

    REQUIRE (map.has_value());
    CHECK (problems.isEmpty());
    CHECK (map->id == "general-midi");
    CHECK (map->isGeneralMidi());
    REQUIRE (map->notes.size() == 47);
    CHECK (map->notes.front().note == 35);
    CHECK (map->notes.back().note == 81);
    for (const auto& n : map->notes)
        CHECK (n.gm == n.note);
    CHECK (map->find (36)->name == "Bass Drum 1");
    CHECK (map->find (81)->name == "Open Triangle");
}
