#include <catch2/catch_test_macros.hpp>

#include <juce_core/juce_core.h>

#include "BinaryData.h"
#include "ConversionTable.h"

using namespace swingers;

namespace
{
DrumMap makeMap (std::vector<MapNote> notes)
{
    // A distinct id per map: the same id on both sides means "A to A" (the identity).
    static int counter = 0;
    DrumMap map;
    map.id = "m" + juce::String (++counter);
    map.name = "M";
    map.notes = std::move (notes);
    return map;
}

DrumMap bundledGm()
{
    juce::String resourceName;
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        if (juce::String (BinaryData::originalFilenames[i]) == "general-midi.swmap.json")
            resourceName = BinaryData::namedResourceList[i];

    REQUIRE (resourceName.isNotEmpty());

    int size = 0;
    const auto* data = BinaryData::getNamedResource (resourceName.toRawUTF8(), size);
    REQUIRE (data != nullptr);

    juce::var json;
    REQUIRE (juce::JSON::parse (juce::String::fromUTF8 (data, size), json).wasOk());

    juce::StringArray problems;
    auto map = drumMapFromJson (json, problems);
    REQUIRE (map.has_value());
    return *map;
}

juce::var parse (const juce::String& text)
{
    juce::var v;
    REQUIRE (juce::JSON::parse (text, v).wasOk());
    return v;
}
}

TEST_CASE ("standard route goes through GM")
{
    const auto from = makeMap ({ { 23, "HH open 2", 46, false }, { 46, "HH open", 46, false } });
    const auto to = makeMap ({ { 26, "HH open", 46, false } });

    const auto table = ConversionTable::build (from, to, nullptr);

    CHECK (table.entries[46].target == 26);
    CHECK (table.entries[46].route == Route::Standard);
    CHECK (table.entries[46].gm == 46);
    CHECK (table.entries[23].target == 26);
    CHECK (table.entries[23].route == Route::Standard);
    CHECK (table.entries[23].gm == 46);
}

TEST_CASE ("preferred wins, otherwise the lowest note")
{
    const auto from = makeMap ({ { 38, "Snare", 38, false } });
    auto to = makeMap ({ { 37, "Snare A", 38, false }, { 38, "Snare B", 38, false }, { 40, "Snare C", 38, false } });

    CHECK (ConversionTable::build (from, to, nullptr).entries[38].target == 37);

    to.setPreferred (40);
    CHECK (ConversionTable::build (from, to, nullptr).entries[38].target == 40);
}

TEST_CASE ("several preferred notes on the same voice: the lowest preferred wins")
{
    const auto from = makeMap ({ { 38, "Snare", 38, false } });
    // A loaded file can carry more than one preferred note per voice; setPreferred would prevent it.
    const auto to = makeMap ({ { 37, "Snare A", 38, false },
                               { 40, "Snare C", 38, true },
                               { 41, "Snare D", 38, true } });

    const auto table = ConversionTable::build (from, to, nullptr);
    CHECK (table.entries[38].target == 40);
    CHECK (table.entries[38].route == Route::Standard);
}

TEST_CASE ("override and drop")
{
    const auto from = makeMap ({ { 60, "Bongo", kNoGm, false }, { 61, "Bongo 2", 61, false } });
    const auto to = makeMap ({ { 33, "Thing", kNoGm, false }, { 61, "Bongo 2", 61, false } });

    PairOverrides overrides;
    overrides.connect (60, 33);
    overrides.drop (61);

    const auto table = ConversionTable::build (from, to, &overrides);

    CHECK (table.entries[60].target == 33);
    CHECK (table.entries[60].route == Route::Override);
    CHECK (table.entries[61].route == Route::DroppedByUser);
    CHECK (table.entries[61].target == -1);
    CHECK (table.entries[61].gm == 61);
}

TEST_CASE ("direct override wins over the standard route, an empty override changes nothing")
{
    const auto from = makeMap ({ { 36, "Kick", 36, false } });
    const auto to = makeMap ({ { 35, "Kick", 36, false }, { 50, "Other", kNoGm, false } });

    PairOverrides none;
    CHECK (ConversionTable::build (from, to, &none).entries[36].target == 35);

    PairOverrides overrides;
    overrides.connect (36, 50);
    const auto table = ConversionTable::build (from, to, &overrides);
    CHECK (table.entries[36].target == 50);
    CHECK (table.entries[36].route == Route::Override);
    CHECK (table.entries[36].gm == 36);
}

TEST_CASE ("no match and unknown notes are dropped")
{
    const auto from = makeMap ({ { 36, "Kick", 36, false }, { 60, "Odd", kNoGm, false } });
    const auto to = makeMap ({ { 38, "Snare", 38, false } });

    const auto table = ConversionTable::build (from, to, nullptr);

    CHECK (table.entries[36].route == Route::NoMatch);
    CHECK (table.entries[36].target == -1);
    CHECK (table.entries[36].gm == 36);
    CHECK (table.entries[60].route == Route::NoMatch);
    CHECK (table.entries[60].target == -1);
    CHECK (table.entries[99].route == Route::NoMatch);
    CHECK (table.entries[99].target == -1);
    CHECK (table.entries[99].gm == kNoGm);
}

TEST_CASE ("GM map on either side")
{
    const auto gm = bundledGm();

    const auto toCustom = makeMap ({ { 26, "HH open", 46, false } });
    CHECK (ConversionTable::build (gm, toCustom, nullptr).entries[46].target == 26);

    const auto fromCustom = makeMap ({ { 23, "HH open 2", 46, false } });
    const auto table = ConversionTable::build (fromCustom, gm, nullptr);
    CHECK (table.entries[23].target == 46);
    CHECK (table.entries[23].route == Route::Standard);
}

TEST_CASE ("the same map on both sides is the identity, pair overrides still apply")
{
    const auto gm = bundledGm();
    const auto gmToGm = ConversionTable::build (gm, gm, nullptr);
    for (int note = 0; note < 128; ++note)
    {
        CHECK (gmToGm.entries[(size_t) note].target == note);
        CHECK (gmToGm.entries[(size_t) note].route == Route::Standard);
    }
    CHECK (gmToGm.entries[22].target == 22); // outside the GM range 35-81
    CHECK (gmToGm.entries[90].target == 90);
    CHECK (gmToGm.entries[38].gm == 38);

    // Two notes on the same GM voice stay distinct (through the pivot both would go to the lowest).
    auto custom = makeMap ({ { 37, "Snare A", 38, false }, { 40, "Snare B", 38, false } });
    const auto aToA = ConversionTable::build (custom, custom, nullptr);
    CHECK (aToA.entries[37].target == 37);
    CHECK (aToA.entries[40].target == 40);
    CHECK (aToA.entries[40].gm == 38);

    PairOverrides overrides;
    overrides.fromId = overrides.toId = custom.id;
    overrides.connect (40, 41);
    overrides.drop (37);
    const auto withOverrides = ConversionTable::build (custom, custom, &overrides);
    CHECK (withOverrides.entries[40].target == 41);
    CHECK (withOverrides.entries[40].route == Route::Override);
    CHECK (withOverrides.entries[37].route == Route::DroppedByUser);
    CHECK (withOverrides.entries[37].target == -1);
    CHECK (withOverrides.entries[60].target == 60);
}

TEST_CASE ("identity")
{
    const auto table = ConversionTable::identity();
    for (int note = 0; note < 128; ++note)
    {
        CHECK (table.entries[(size_t) note].target == note);
        CHECK (table.entries[(size_t) note].route == Route::Standard);
        CHECK (table.entries[(size_t) note].gm == note);
    }
}

TEST_CASE ("pair overrides round trip")
{
    PairOverrides overrides;
    overrides.fromId = "a";
    overrides.toId = "b";
    overrides.connect (60, 33);
    overrides.connect (62, 34);
    overrides.drop (61);

    juce::StringArray problems;
    const auto back = pairOverridesFromJson (toJson (overrides), problems);

    REQUIRE (back.has_value());
    CHECK (problems.isEmpty());
    CHECK (back->fromId == "a");
    CHECK (back->toId == "b");
    CHECK (back->direct == overrides.direct);
    CHECK (back->dropped == overrides.dropped);
}

TEST_CASE ("connect, drop and reset keep the two sets disjoint")
{
    PairOverrides overrides;
    CHECK (overrides.empty());

    overrides.connect (60, 33);
    overrides.drop (61);
    CHECK_FALSE (overrides.empty());

    overrides.drop (60);
    CHECK (overrides.direct.count (60) == 0);
    CHECK (overrides.dropped.count (60) == 1);

    overrides.connect (61, 40);
    CHECK (overrides.dropped.count (61) == 0);
    CHECK (overrides.direct.at (61) == 40);

    overrides.reset (60);
    overrides.reset (61);
    CHECK (overrides.empty());
}

TEST_CASE ("pair overrides from JSON skip and report bad entries")
{
    const auto json = parse (R"({"format":"swingers-pair","version":1,"from":"a","to":"b",
        "direct":[{"note":60,"to":33},{"note":200,"to":1},{"note":61,"to":128},{"note":60,"to":34},{"note":"x"},{"note":70,"to":71}],
        "dropped":[70,80,-1,"y"]})");

    juce::StringArray problems;
    const auto result = pairOverridesFromJson (json, problems);

    REQUIRE (result.has_value());
    CHECK (problems.size() == 7);
    CHECK (result->direct.size() == 2);
    CHECK (result->direct.at (60) == 33);
    CHECK (result->direct.at (70) == 71);
    CHECK (result->dropped == std::set<int> { 80 });
}

TEST_CASE ("pair overrides from JSON reject wrong documents")
{
    juce::StringArray problems;
    CHECK_FALSE (pairOverridesFromJson (parse (R"({"format":"swingers-map","version":1,"from":"a","to":"b"})"), problems).has_value());
    CHECK_FALSE (pairOverridesFromJson (parse (R"({"format":"swingers-pair","version":2,"from":"a","to":"b"})"), problems).has_value());
    CHECK_FALSE (pairOverridesFromJson (parse (R"({"format":"swingers-pair","version":1,"to":"b"})"), problems).has_value());
    CHECK_FALSE (pairOverridesFromJson (juce::var (3), problems).has_value());
}
