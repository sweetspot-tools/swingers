#include <catch2/catch_test_macros.hpp>

#include <juce_core/juce_core.h>

#include "CubaseImport.h"

using namespace swingers;

namespace
{
juce::String fixture (const char* name)
{
    const auto file = juce::File (SWINGERS_FIXTURES_DIR).getChildFile (name);
    REQUIRE (file.existsAsFile());
    return file.loadFileAsString();
}
}

TEST_CASE ("imports notes, names and GM voices")
{
    juce::String error;
    const auto map = importCubaseDrumMap (fixture ("cubase_basic.xml"), error);
    REQUIRE (map.has_value());

    CHECK (map->id == "test-kit");
    CHECK (map->name == "Test Kit");
    CHECK (map->library == "Test Kit");

    REQUIRE (map->notes.size() == 4);
    CHECK (map->notes[0].note == 36);
    CHECK (map->notes[0].name == "Kick");
    CHECK (map->notes[0].gm == 36);
    CHECK (map->notes[1].note == 38);
    CHECK (map->notes[1].name == "Snare");
    CHECK (map->notes[1].gm == 38);
    CHECK (map->notes[2].note == 40);
    CHECK (map->notes[2].name == "Snare Rim");
    CHECK (map->notes[2].gm == 38);
    CHECK (map->notes[3].note == 47);
    CHECK (map->notes[3].name == "Xtra A");
    CHECK (map->notes[3].gm == kNoGm);

    CHECK (map->find (50) == nullptr);
    CHECK (map->find (51) == nullptr);
}

TEST_CASE ("tolerates unescaped characters in names")
{
    juce::String error;
    const auto map = importCubaseDrumMap (fixture ("cubase_unescaped.xml"), error);
    REQUIRE (map.has_value());

    REQUIRE (map->find (84) != nullptr);
    REQUIRE (map->find (85) != nullptr);
    CHECK (map->find (84)->name == "[Ride ><] *Ride Tip");
    CHECK (map->find (85)->name == "Bell & Tip");
}

TEST_CASE ("keeps entities that are already escaped")
{
    juce::String error;
    const auto map = importCubaseDrumMap (
        "<DrumMap><string name=\"Name\" value=\"A &amp; B\"/><list name=\"Map\" type=\"list\">"
        "<item><int name=\"INote\" value=\"36\"/><int name=\"ONote\" value=\"36\"/>"
        "<string name=\"Name\" value=\"Tom &lt;1&gt; &#38; &amp;co\"/></item></list></DrumMap>",
        error);
    REQUIRE (map.has_value());
    CHECK (map->name == "A & B");
    CHECK (map->id == "a-b");
    CHECK (map->find (36)->name == "Tom <1> & &co");
}

TEST_CASE ("skips voices out of range and keeps the first of a duplicate note")
{
    juce::String error;
    const auto map = importCubaseDrumMap (
        "<DrumMap><string name=\"Name\" value=\"X\"/><list name=\"Map\" type=\"list\">"
        "<item><int name=\"INote\" value=\"40\"/><int name=\"ONote\" value=\"38\"/><string name=\"Name\" value=\"B\"/></item>"
        "<item><int name=\"INote\" value=\"128\"/><int name=\"ONote\" value=\"38\"/><string name=\"Name\" value=\"High\"/></item>"
        "<item><int name=\"INote\" value=\"41\"/><int name=\"ONote\" value=\"200\"/><string name=\"Name\" value=\"BadOut\"/></item>"
        "<item><int name=\"ONote\" value=\"38\"/><string name=\"Name\" value=\"NoIn\"/></item>"
        "<item><int name=\"INote\" value=\"36\"/><int name=\"ONote\" value=\"36\"/><string name=\"Name\" value=\"A\"/></item>"
        "<item><int name=\"INote\" value=\"40\"/><int name=\"ONote\" value=\"39\"/><string name=\"Name\" value=\"Dup\"/></item>"
        "</list></DrumMap>",
        error);
    REQUIRE (map.has_value());
    REQUIRE (map->notes.size() == 2);
    CHECK (map->notes[0].note == 36);
    CHECK (map->notes[1].note == 40);
    CHECK (map->notes[1].name == "B");
    CHECK (map->notes[1].gm == 38);
}

TEST_CASE ("rejects files without a Map list")
{
    juce::String error;
    CHECK_FALSE (importCubaseDrumMap ("<DrumMap/>", error).has_value());
    CHECK (error.isNotEmpty());

    error.clear();
    CHECK_FALSE (importCubaseDrumMap ("<DrumMap><string name=\"Name\" value=\"X\"/></DrumMap>", error).has_value());
    CHECK (error.isNotEmpty());

    error.clear();
    CHECK_FALSE (importCubaseDrumMap ("not xml at all", error).has_value());
    CHECK (error.isNotEmpty());
}
