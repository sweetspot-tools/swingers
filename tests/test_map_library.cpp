#include <catch2/catch_test_macros.hpp>

#include <juce_core/juce_core.h>

#include "DrumMap.h"
#include "MapLibrary.h"
#include "PairOverrides.h"

using namespace swingers;

namespace
{
struct TempRoot
{
    juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("swingers-library-" + juce::Uuid().toString());
    ~TempRoot() { dir.deleteRecursively(); }
};

DrumMap userMap (const juce::String& id, const juce::String& name)
{
    DrumMap map;
    map.id = id;
    map.name = name;
    map.library = "Test";
    map.notes = { { 36, "Kick", 36, false } };
    return map;
}

void writeText (const juce::File& file, const juce::String& text)
{
    REQUIRE (file.getParentDirectory().createDirectory());
    REQUIRE (file.replaceWithText (text));
}

juce::File fixture (const char* name)
{
    return juce::File (SWINGERS_FIXTURES_DIR).getChildFile (name);
}
}

TEST_CASE ("loads bundled and user maps")
{
    TempRoot tmp;
    {
        MapLibrary writer (tmp.dir);
        REQUIRE (writer.saveMap (userMap ("my-kit", "My Kit")).wasOk());
    }

    MapLibrary library (tmp.dir);

    REQUIRE (library.maps().size() == 2);
    CHECK (library.maps()[0].id == "general-midi");
    CHECK (library.maps()[1].id == "my-kit");
    CHECK (library.isBundled ("general-midi"));
    CHECK_FALSE (library.isBundled ("my-kit"));
    CHECK (library.find ("my-kit") != nullptr);
    CHECK (library.find ("nope") == nullptr);
    CHECK (library.issues().empty());
}

TEST_CASE ("user maps are sorted by name ignoring case, ties by id")
{
    TempRoot tmp;
    MapLibrary library (tmp.dir);
    // file-name order (a, y, z) differs from the expected order (y, z, a)
    REQUIRE (library.saveMap (userMap ("z", "Alpha")).wasOk());
    REQUIRE (library.saveMap (userMap ("y", "alpha")).wasOk());
    REQUIRE (library.saveMap (userMap ("a", "Zeta")).wasOk());

    REQUIRE (library.maps().size() == 4);
    CHECK (library.maps()[0].id == "general-midi");
    CHECK (library.maps()[1].id == "y");
    CHECK (library.maps()[2].id == "z");
    CHECK (library.maps()[3].id == "a");
}

TEST_CASE ("broken files are skipped and reported")
{
    TempRoot tmp;
    writeText (tmp.dir.getChildFile ("maps/broken.swmap.json"), "{ not json");
    writeText (tmp.dir.getChildFile ("maps/wrong.swmap.json"), R"({"format":"other","version":1})");
    writeText (tmp.dir.getChildFile ("pairs/a__b.swpair.json"), "[[[");

    MapLibrary library (tmp.dir);

    CHECK (library.maps().size() == 1);
    REQUIRE (library.issues().size() == 3);
    juce::StringArray files;
    for (const auto& issue : library.issues())
    {
        files.add (issue.file);
        CHECK (issue.message.isNotEmpty());
    }
    CHECK (files.contains ("broken.swmap.json"));
    CHECK (files.contains ("wrong.swmap.json"));
    CHECK (files.contains ("a__b.swpair.json"));
}

TEST_CASE ("bundled ids cannot be overwritten")
{
    TempRoot tmp;
    MapLibrary library (tmp.dir);
    const auto original = *library.find ("general-midi");

    DrumMap fake = userMap ("general-midi", "Fake GM");
    CHECK (library.saveMap (fake).failed());
    CHECK_FALSE (tmp.dir.getChildFile ("maps/general-midi.swmap.json").exists());

    // a stray file with a bundled id is ignored and reported
    writeText (tmp.dir.getChildFile ("maps/general-midi.swmap.json"), juce::JSON::toString (toJson (fake)));
    library.reload();

    REQUIRE (library.maps().size() == 1);
    CHECK (library.find ("general-midi")->name == original.name);
    REQUIRE (library.issues().size() == 1);
    CHECK (library.issues()[0].file == "general-midi.swmap.json");
}

TEST_CASE ("a map file whose name does not match its id is reported")
{
    TempRoot tmp;
    writeText (tmp.dir.getChildFile ("maps/other.swmap.json"), juce::JSON::toString (toJson (userMap ("my-kit", "My Kit"))));

    MapLibrary library (tmp.dir);

    CHECK (library.maps().size() == 1);
    REQUIRE (library.issues().size() == 1);
    CHECK (library.issues()[0].file == "other.swmap.json");
}

TEST_CASE ("a map with an invalid id is reported")
{
    TempRoot tmp;
    writeText (tmp.dir.getChildFile ("maps/My_Kit.swmap.json"), juce::JSON::toString (toJson (userMap ("My_Kit", "My Kit"))));

    MapLibrary library (tmp.dir);

    CHECK (library.maps().size() == 1);
    REQUIRE (library.issues().size() == 1);
    CHECK (library.issues()[0].file == "My_Kit.swmap.json");
    CHECK (library.issues()[0].message.containsIgnoreCase ("invalid id"));
}

TEST_CASE ("second import of the same name gets -2")
{
    TempRoot tmp;
    MapLibrary library (tmp.dir);

    juce::String first, second;
    REQUIRE (library.importCubase (fixture ("cubase_basic.xml"), first).wasOk());
    REQUIRE (library.importCubase (fixture ("cubase_basic.xml"), second).wasOk());

    CHECK (first == "test-kit");
    CHECK (second == "test-kit-2");
    CHECK (tmp.dir.getChildFile ("maps/test-kit.swmap.json").existsAsFile());
    CHECK (tmp.dir.getChildFile ("maps/test-kit-2.swmap.json").existsAsFile());
    CHECK (library.find ("test-kit-2") != nullptr);
    CHECK (library.uniqueId ("test-kit") == "test-kit-3");
    CHECK (library.uniqueId ("general-midi") == "general-midi-2");
    CHECK (library.uniqueId ("fresh") == "fresh");
}

TEST_CASE ("import reports readable errors")
{
    TempRoot tmp;
    MapLibrary library (tmp.dir);
    juce::String id;

    const auto missing = library.importCubase (tmp.dir.getChildFile ("missing.xml"), id);
    CHECK (missing.failed());
    CHECK (missing.getErrorMessage().isNotEmpty());

    writeText (tmp.dir.getChildFile ("bad.xml"), "not a drum map");
    const auto bad = library.importCubase (tmp.dir.getChildFile ("bad.xml"), id);
    CHECK (bad.failed());
    CHECK (bad.getErrorMessage().isNotEmpty());
    CHECK (library.maps().size() == 1);
}

TEST_CASE ("pair file exists only with overrides")
{
    TempRoot tmp;
    MapLibrary library (tmp.dir);
    const auto file = tmp.dir.getChildFile ("pairs/test-kit__general-midi.swpair.json");

    CHECK (library.overrides ("test-kit", "general-midi").empty());
    CHECK (library.overrides ("test-kit", "general-midi").fromId == "test-kit");
    CHECK (library.overrides ("test-kit", "general-midi").toId == "general-midi");

    PairOverrides pair;
    pair.fromId = "test-kit";
    pair.toId = "general-midi";
    pair.connect (60, 38);
    REQUIRE (library.saveOverrides (pair).wasOk());

    CHECK (file.existsAsFile());
    const auto loaded = library.overrides ("test-kit", "general-midi");
    CHECK (loaded.direct.at (60) == 38);

    pair.reset (60);
    REQUIRE (library.saveOverrides (pair).wasOk());

    CHECK_FALSE (file.exists());
    CHECK (library.overrides ("test-kit", "general-midi").empty());
}

TEST_CASE ("ids that are not safe file names are refused")
{
    TempRoot tmp;
    MapLibrary library (tmp.dir);

    CHECK (library.saveMap (userMap ("../evil", "Evil")).failed());
    CHECK (library.saveMap (userMap ("", "Empty")).failed());
    CHECK (library.maps().size() == 1);
}

TEST_CASE ("ids must be lowercase letters, digits and dashes")
{
    TempRoot tmp;
    MapLibrary library (tmp.dir);

    CHECK (library.saveMap (userMap ("My-Kit", "Upper")).failed());
    CHECK (library.saveMap (userMap ("my_kit", "Underscore")).failed());
    CHECK (library.saveMap (userMap ("my.kit", "Dot")).failed());
    CHECK (library.saveMap (userMap ("my-kit-2", "Ok")).wasOk());
}

TEST_CASE ("saveOverrides refuses invalid ids")
{
    TempRoot tmp;
    MapLibrary library (tmp.dir);

    PairOverrides pair;
    pair.fromId = "a__b";
    pair.toId = "c";
    pair.connect (60, 38);

    CHECK (library.saveOverrides (pair).failed());
    CHECK_FALSE (tmp.dir.getChildFile ("pairs").exists());
}

TEST_CASE ("a pair file with valid JSON but the wrong format is reported")
{
    TempRoot tmp;
    writeText (tmp.dir.getChildFile ("pairs/a__b.swpair.json"), R"({"format":"swingers-map","version":1})");

    MapLibrary library (tmp.dir);

    REQUIRE (library.issues().size() == 1);
    CHECK (library.issues()[0].file == "a__b.swpair.json");
    CHECK (library.overrides ("a", "b").empty());
}

TEST_CASE ("a pair file whose name does not match its ids is reported")
{
    TempRoot tmp;
    PairOverrides pair;
    pair.fromId = "a";
    pair.toId = "b";
    pair.connect (60, 38);
    writeText (tmp.dir.getChildFile ("pairs/x__y.swpair.json"), juce::JSON::toString (toJson (pair)));

    MapLibrary library (tmp.dir);

    REQUIRE (library.issues().size() == 1);
    CHECK (library.issues()[0].file == "x__y.swpair.json");
    CHECK (library.overrides ("a", "b").empty());
}

TEST_CASE ("never overwrites a map file that did not load")
{
    TempRoot tmp;
    const auto broken = tmp.dir.getChildFile ("maps/test-kit.swmap.json");
    writeText (broken, "{ not json");

    MapLibrary library (tmp.dir);

    // import picks a free id and leaves the broken file alone
    juce::String id;
    REQUIRE (library.importCubase (fixture ("cubase_basic.xml"), id).wasOk());
    CHECK (id == "test-kit-2");
    CHECK (broken.loadFileAsString() == "{ not json");
    CHECK (library.uniqueId ("test-kit") == "test-kit-3");

    // a direct save with that id is refused
    const auto result = library.saveMap (userMap ("test-kit", "Mine"));
    CHECK (result.failed());
    CHECK (result.getErrorMessage().isNotEmpty());
    CHECK (broken.loadFileAsString() == "{ not json");

    // re-saving a loaded user map is fine
    CHECK (library.saveMap (*library.find ("test-kit-2")).wasOk());
}

TEST_CASE ("saveOverrides refuses to replace a pair file that did not load")
{
    TempRoot tmp;
    const auto broken = tmp.dir.getChildFile ("pairs/a__b.swpair.json");
    writeText (broken, "[[[");

    MapLibrary library (tmp.dir);

    PairOverrides pair;
    pair.fromId = "a";
    pair.toId = "b";
    pair.connect (60, 38);
    CHECK (library.saveOverrides (pair).failed());
    CHECK (broken.loadFileAsString() == "[[[");

    PairOverrides none;
    none.fromId = "a";
    none.toId = "b";
    CHECK (library.saveOverrides (none).failed());
    CHECK (broken.existsAsFile());
}
