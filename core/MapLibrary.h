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
    juce::Result importCubase (const juce::File& xml, juce::String& newId);

    // Starting points for the map editor; neither is saved. newMap: id uniqueId("new-map"), name "New map", no
    // notes. duplicate: id uniqueId(id + "-copy"), name "<name> (copy)", the same notes; an empty map (empty id)
    // when `id` is unknown.
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
};
} // namespace swingers
