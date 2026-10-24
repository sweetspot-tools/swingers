#pragma once

#include "DrumMap.h"
#include "PairOverrides.h"

#include <juce_core/juce_core.h>

#include <vector>

namespace swingers
{
struct LoadIssue
{
    juce::String file, message;
};

// Bundled maps (BinaryData) plus the user's maps and pair-override files found under `userRoot`:
//   userRoot/maps/<id>.swmap.json, userRoot/pairs/<fromId>__<toId>.swpair.json
// Not thread safe: use it from the message thread only.
// A valid id is [a-z0-9-]+. A user file must be named exactly after its id(s), otherwise it is reported.
// maps(), find() and issues() return references/pointers into internal storage that reload(), saveMap,
// saveOverrides and importCubase invalidate: callers copy, or look the map up again by id.
class MapLibrary
{
public:
    explicit MapLibrary (juce::File userRoot);

    static juce::File defaultUserRoot(); // ~/Documents/Swingers

    void reload();

    const std::vector<DrumMap>& maps() const { return allMaps; } // bundled first, then user maps by name
    const DrumMap* find (const juce::String& id) const;
    bool isBundled (const juce::String& id) const;
    const std::vector<LoadIssue>& issues() const { return loadIssues; }

    // Empty overrides (with fromId/toId set) when there is no file for the pair.
    PairOverrides overrides (const juce::String& fromId, const juce::String& toId) const;

    juce::Result saveMap (const DrumMap&);             // refuses bundled ids
    juce::Result saveOverrides (const PairOverrides&); // empty overrides delete the pair file

    juce::String uniqueId (const juce::String& base) const; // base, base-2, base-3...

    // A free id for a map called `name`: slug of the name (as for Cubase imports, "map" when empty, at most 40
    // characters) + "-" + 4 random lowercase hex digits, e.g. "studio-kit-7f3a". Drawn again while the id is in
    // the library or its file exists on disk; uniqueId() as the last resort. Uses the library as loaded.
    juce::String freshId (const juce::String& name);
    void setRandomSeed (juce::int64 seed) { random.setSeed (seed); } // tests: makes freshId predictable

    // Saves a map that is not in the library yet (new, duplicated): reloads first (another instance may have
    // saved meanwhile), gives `map` its id with freshId(map.name), then saveMap. `map.id` is the saved id.
    juce::Result saveNewMap (DrumMap& map);

    // Reloads, then saves the Cubase map with freshId(name of the map). `newId` is the saved id.
    juce::Result importCubase (const juce::File& xml, juce::String& newId);

    // Starting points for the map editor; neither is saved and neither has an id yet (it comes with
    // saveNewMap). newMap: name "New map", no notes. duplicate: name "<name> (copy)", the same notes; an empty
    // map with an empty name when `id` is unknown.
    DrumMap newMap() const;
    DrumMap duplicate (const juce::String& id) const;

private:
    juce::File mapsDir() const { return root.getChildFile ("maps"); }
    juce::File pairsDir() const { return root.getChildFile ("pairs"); }

    juce::File root;
    size_t bundledCount = 0;
    std::vector<DrumMap> allMaps;
    std::vector<PairOverrides> pairs;
    std::vector<LoadIssue> loadIssues;
    juce::Random random;
};
} // namespace swingers
