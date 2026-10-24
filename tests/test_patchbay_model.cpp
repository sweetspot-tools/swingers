#include <catch2/catch_test_macros.hpp>

#include "PatchbayModel.h"

using namespace swingers;

namespace
{
DrumMap makeMap (const char* id, std::vector<MapNote> notes)
{
    DrumMap map;
    map.id = id;
    map.name = id;
    map.notes = std::move (notes);
    return map;
}

PatchbayModel build (const DrumMap& from, const DrumMap& to, const PairOverrides* overrides = nullptr,
                     bool allNotes = false)
{
    return PatchbayModel::build (from, to, ConversionTable::build (from, to, overrides), allNotes);
}
} // namespace

TEST_CASE ("patchbay: standard cables pass through GM")
{
    const auto from = makeMap ("from", { { 23, "HH Open 2", 46, false }, { 46, "HH Open", 46, false } });
    const auto to = makeMap ("to", { { 26, "HH Open", 46, false } });

    const auto model = build (from, to);
    REQUIRE (model.cables.size() == 2);
    CHECK (model.cables[0] == PatchCable { 23, 46, 26, false });
    CHECK (model.cables[1] == PatchCable { 46, 46, 26, false });
    CHECK (model.gmVoices == std::vector<int> { 46 });
    CHECK (model.showGmColumn);
    REQUIRE (model.sources.size() == 2);
    CHECK (model.sources[0].note == 23);
    CHECK (model.sources[0].state == SourceState::Connected);
    REQUIRE (model.targets.size() == 1);
    CHECK (model.targets[0].note == 26);
}

TEST_CASE ("patchbay: a direct cable replaces the standard one")
{
    const auto from = makeMap ("from", { { 23, "HH Open 2", 46, false }, { 46, "HH Open", 46, false } });
    const auto to = makeMap ("to", { { 26, "HH Open", 46, false }, { 30, "Other", 49, false } });

    PairOverrides overrides;
    overrides.connect (23, 30);
    const auto model = build (from, to, &overrides);

    REQUIRE (model.cables.size() == 2);
    CHECK (model.cables[0] == PatchCable { 23, kNoGm, 30, true });
    CHECK (model.cables[1] == PatchCable { 46, 46, 26, false });
    CHECK (model.gmVoices == std::vector<int> { 46 }); // 23 no longer goes through GM
}

TEST_CASE ("patchbay: unmatched and dropped sources have no cable")
{
    const auto from = makeMap ("from", { { 36, "Kick", 36, false }, { 40, "Flam", kNoGm, false },
                                         { 42, "HH", 42, false }, { 46, "Open", 46, false } });
    const auto to = makeMap ("to", { { 36, "Kick", 36, false }, { 42, "HH", 42, false } });

    PairOverrides overrides;
    overrides.drop (42);
    const auto model = build (from, to, &overrides);

    REQUIRE (model.sources.size() == 4);
    CHECK (model.sources[0].state == SourceState::Connected);
    CHECK (model.sources[1].state == SourceState::NoMatch);      // no GM voice
    CHECK (model.sources[2].state == SourceState::DroppedByUser);
    CHECK (model.sources[3].state == SourceState::NoMatch);      // voice 46 missing in `to`

    REQUIRE (model.cables.size() == 1);
    CHECK (model.cables[0].source == 36);
    CHECK (model.gmVoices == std::vector<int> { 36 });
}

TEST_CASE ("patchbay: no GM column when a side is GM")
{
    const auto gm = makeMap ("general-midi", { { 36, "Bass Drum 1", 36, false }, { 38, "Acoustic Snare", 38, false } });
    const auto other = makeMap ("other", { { 35, "Kick", 36, false }, { 38, "Snare", 38, false } });

    const auto fromGm = build (gm, other);
    CHECK_FALSE (fromGm.showGmColumn);
    CHECK (fromGm.gmVoices.empty());
    CHECK (fromGm.cables.size() == 2);

    const auto toGm = build (other, gm);
    CHECK_FALSE (toGm.showGmColumn);
    CHECK (toGm.gmVoices.empty());
    CHECK (toGm.cables.size() == 2);

    CHECK (build (other, other).showGmColumn);
}

TEST_CASE ("patchbay: allNotes lists 128 rows")
{
    const auto from = makeMap ("from", { { 36, "Kick", 36, false } });
    const auto to = makeMap ("to", { { 35, "Kick", 36, true } });

    const auto few = build (from, to);
    CHECK (few.sources.size() == 1);
    CHECK (few.targets.size() == 1);

    const auto all = build (from, to, nullptr, true);
    REQUIRE (all.sources.size() == 128);
    REQUIRE (all.targets.size() == 128);
    for (int i = 0; i < 128; ++i)
    {
        CHECK (all.sources[(size_t) i].note == i);
        CHECK (all.targets[(size_t) i].note == i);
    }
    CHECK (all.sources[36].name == "Kick");
    CHECK (all.sources[36].state == SourceState::Connected);
    CHECK (all.sources[37].name.isEmpty());
    CHECK (all.sources[37].state == SourceState::NoMatch);
    CHECK (all.targets[35].name == "Kick");
    CHECK (all.targets[35].preferred);
    CHECK (all.targets[0].name.isEmpty());
    CHECK (all.cables.size() == 1);
}

TEST_CASE ("patchbay: overrides on notes the map does not define show up in allNotes")
{
    const auto from = makeMap ("from", { { 36, "Kick", 36, false } });
    const auto to = makeMap ("to", { { 35, "Kick", 36, false } });

    PairOverrides overrides;
    overrides.connect (60, 35);
    overrides.drop (61);
    const auto all = build (from, to, &overrides, true);

    CHECK (all.sources[60].state == SourceState::Connected);
    CHECK (all.sources[61].state == SourceState::DroppedByUser);
    bool found = false;
    for (const auto& c : all.cables)
        found = found || c == PatchCable { 60, kNoGm, 35, true };
    CHECK (found);
    CHECK (all.targetIndexOf (35) == 35);
    CHECK (build (from, to).targetIndexOf (36) == -1);
}
