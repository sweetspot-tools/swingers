#include "MapLibrary.h"

#include "BinaryData.h"
#include "CubaseImport.h"

#include <algorithm>

namespace swingers
{
namespace
{
constexpr const char* kMapSuffix = ".swmap.json";
constexpr const char* kPairSuffix = ".swpair.json";

// Ids become file names: keep them to a safe alphabet.
bool isSafeId (const juce::String& id)
{
    if (id.isEmpty())
        return false;
    for (auto c : id)
        if (! (juce::CharacterFunctions::isLetterOrDigit (c) || c == '-' || c == '_'))
            return false;
    return true;
}

juce::Result writeJson (const juce::File& file, const juce::var& json)
{
    if (const auto dir = file.getParentDirectory(); ! dir.createDirectory())
        return juce::Result::fail ("Cannot create folder " + dir.getFullPathName());

    juce::TemporaryFile temp (file);
    if (! temp.getFile().replaceWithText (juce::JSON::toString (json), false, false, "\n"))
        return juce::Result::fail ("Cannot write " + file.getFileName());
    if (! temp.overwriteTargetFileWithTemporary())
        return juce::Result::fail ("Cannot replace " + file.getFileName());
    return juce::Result::ok();
}

juce::File mapFile (const juce::File& dir, const juce::String& id)
{
    return dir.getChildFile (id + kMapSuffix);
}

juce::File pairFile (const juce::File& dir, const juce::String& fromId, const juce::String& toId)
{
    return dir.getChildFile (fromId + "__" + toId + kPairSuffix);
}
} // namespace

MapLibrary::MapLibrary (juce::File userRoot) : root (std::move (userRoot))
{
    reload();
}

juce::File MapLibrary::defaultUserRoot()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Swingers");
}

void MapLibrary::reload()
{
    allMaps.clear();
    pairs.clear();
    loadIssues.clear();

    // Bundled maps, in BinaryData order.
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const juce::String original (BinaryData::originalFilenames[i]);
        if (! original.endsWith (kMapSuffix))
            continue;

        int size = 0;
        const auto* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);
        juce::var json;
        juce::StringArray problems;
        if (data == nullptr || ! juce::JSON::parse (juce::String::fromUTF8 (data, size), json).wasOk())
            continue;
        if (auto map = drumMapFromJson (json, problems))
            allMaps.push_back (std::move (*map));
    }
    bundledCount = allMaps.size();

    // User maps: by file name, so that on a duplicate id the first file wins.
    std::vector<DrumMap> userMaps;
    auto mapFiles = mapsDir().findChildFiles (juce::File::findFiles, false, juce::String ("*") + kMapSuffix);
    std::sort (mapFiles.begin(), mapFiles.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileName().compareNatural (b.getFileName()) < 0; });

    for (const auto& file : mapFiles)
    {
        juce::var json;
        if (const auto parsed = juce::JSON::parse (file.loadFileAsString(), json); parsed.failed())
        {
            loadIssues.push_back ({ file.getFileName(), "Invalid JSON: " + parsed.getErrorMessage() });
            continue;
        }

        juce::StringArray problems;
        auto map = drumMapFromJson (json, problems);
        if (! map)
        {
            loadIssues.push_back ({ file.getFileName(), "Not a valid Swingers drum map" });
            continue;
        }
        if (isBundled (map->id))
        {
            loadIssues.push_back ({ file.getFileName(), "The id \"" + map->id + "\" belongs to a bundled map" });
            continue;
        }
        if (std::any_of (userMaps.begin(), userMaps.end(), [&] (const DrumMap& m) { return m.id == map->id; }))
        {
            loadIssues.push_back ({ file.getFileName(), "The id \"" + map->id + "\" is already used by another file" });
            continue;
        }
        userMaps.push_back (std::move (*map));
    }

    std::stable_sort (userMaps.begin(), userMaps.end(), [] (const DrumMap& a, const DrumMap& b)
                      {
                          if (const auto c = a.name.compareIgnoreCase (b.name); c != 0)
                              return c < 0;
                          return a.id.compare (b.id) < 0;
                      });
    for (auto& m : userMaps)
        allMaps.push_back (std::move (m));

    // Pair overrides.
    auto pairFiles = pairsDir().findChildFiles (juce::File::findFiles, false, juce::String ("*") + kPairSuffix);
    std::sort (pairFiles.begin(), pairFiles.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileName().compareNatural (b.getFileName()) < 0; });

    for (const auto& file : pairFiles)
    {
        juce::var json;
        if (const auto parsed = juce::JSON::parse (file.loadFileAsString(), json); parsed.failed())
        {
            loadIssues.push_back ({ file.getFileName(), "Invalid JSON: " + parsed.getErrorMessage() });
            continue;
        }

        juce::StringArray problems;
        auto loaded = pairOverridesFromJson (json, problems);
        if (! loaded)
        {
            loadIssues.push_back ({ file.getFileName(), "Not a valid Swingers pair file" });
            continue;
        }
        pairs.push_back (std::move (*loaded));
    }
}

const DrumMap* MapLibrary::find (const juce::String& id) const
{
    for (const auto& m : allMaps)
        if (m.id == id)
            return &m;
    return nullptr;
}

bool MapLibrary::isBundled (const juce::String& id) const
{
    for (size_t i = 0; i < bundledCount && i < allMaps.size(); ++i)
        if (allMaps[i].id == id)
            return true;
    return false;
}

PairOverrides MapLibrary::overrides (const juce::String& fromId, const juce::String& toId) const
{
    for (const auto& p : pairs)
        if (p.fromId == fromId && p.toId == toId)
            return p;

    PairOverrides none;
    none.fromId = fromId;
    none.toId = toId;
    return none;
}

juce::Result MapLibrary::saveMap (const DrumMap& map)
{
    if (isBundled (map.id))
        return juce::Result::fail ("\"" + map.id + "\" is a bundled map and cannot be overwritten");
    if (! isSafeId (map.id))
        return juce::Result::fail ("The map id may only contain letters, digits, '-' and '_'");

    if (const auto r = writeJson (mapFile (mapsDir(), map.id), toJson (map)); r.failed())
        return r;

    reload();
    return juce::Result::ok();
}

juce::Result MapLibrary::saveOverrides (const PairOverrides& overridesToSave)
{
    if (! isSafeId (overridesToSave.fromId) || ! isSafeId (overridesToSave.toId))
        return juce::Result::fail ("The map ids may only contain letters, digits, '-' and '_'");

    const auto file = pairFile (pairsDir(), overridesToSave.fromId, overridesToSave.toId);

    if (overridesToSave.empty())
    {
        if (file.existsAsFile() && ! file.deleteFile())
            return juce::Result::fail ("Cannot delete " + file.getFileName());
    }
    else if (const auto r = writeJson (file, toJson (overridesToSave)); r.failed())
    {
        return r;
    }

    reload();
    return juce::Result::ok();
}

juce::String MapLibrary::uniqueId (const juce::String& base) const
{
    if (find (base) == nullptr)
        return base;

    for (int n = 2;; ++n)
    {
        const auto candidate = base + "-" + juce::String (n);
        if (find (candidate) == nullptr)
            return candidate;
    }
}

juce::Result MapLibrary::importCubase (const juce::File& xml, juce::String& newId)
{
    if (! xml.existsAsFile())
        return juce::Result::fail ("File not found: " + xml.getFullPathName());

    juce::String error;
    auto map = importCubaseDrumMap (xml.loadFileAsString(), error);
    if (! map)
        return juce::Result::fail (error.isNotEmpty() ? error : "Not a usable Cubase drum map");

    map->id = uniqueId (map->id);
    if (const auto r = saveMap (*map); r.failed())
        return r;

    newId = map->id;
    return juce::Result::ok();
}
} // namespace swingers
