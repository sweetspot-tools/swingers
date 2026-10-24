#include "DrumMap.h"

#include <algorithm>
#include <cmath>

namespace swingers
{
namespace
{
const juce::Identifier kFormat { "format" }, kVersion { "version" }, kId { "id" }, kName { "name" },
    kLibrary { "library" }, kNotes { "notes" }, kNote { "note" }, kGm { "gm" }, kPreferred { "preferred" };

constexpr auto kFormatName = "swingers-map";
constexpr int kFileVersion = 1;

bool isNumber (const juce::var& v)
{
    return v.isInt() || v.isInt64() || v.isDouble();
}

bool asInt (const juce::var& v, int& out)
{
    if (! isNumber (v))
        return false;

    const double d = v;
    if (d != std::floor (d) || d < -1.0e9 || d > 1.0e9)
        return false;

    out = (int) d;
    return true;
}
} // namespace

bool DrumMap::isGeneralMidi() const
{
    return id == kGeneralMidiId;
}

const MapNote* DrumMap::find (int note) const
{
    const auto it = std::find_if (notes.begin(), notes.end(), [note] (const MapNote& n) { return n.note == note; });
    return it != notes.end() ? &*it : nullptr;
}

int DrumMap::gmVoiceOf (int note) const
{
    const auto* n = find (note);
    return n != nullptr ? n->gm : kNoGm;
}

void DrumMap::setPreferred (int note)
{
    MapNote* target = nullptr;
    for (auto& n : notes)
        if (n.note == note)
            target = &n;

    if (target == nullptr)
        return;

    if (target->gm != kNoGm)
        for (auto& n : notes)
            if (n.gm == target->gm)
                n.preferred = false;

    target->preferred = true;
}

juce::var toJson (const DrumMap& map)
{
    juce::Array<juce::var> notes;
    for (const auto& n : map.notes)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty (kNote, n.note);
        o->setProperty (kName, n.name);
        if (n.gm != kNoGm)
            o->setProperty (kGm, n.gm);
        if (n.preferred)
            o->setProperty (kPreferred, true);
        notes.add (juce::var (o));
    }

    auto* root = new juce::DynamicObject();
    root->setProperty (kFormat, kFormatName);
    root->setProperty (kVersion, kFileVersion);
    root->setProperty (kId, map.id);
    root->setProperty (kName, map.name);
    root->setProperty (kLibrary, map.library);
    root->setProperty (kNotes, notes);
    return juce::var (root);
}

std::optional<DrumMap> drumMapFromJson (const juce::var& json, juce::StringArray& problems)
{
    auto* root = json.getDynamicObject();
    if (root == nullptr)
    {
        problems.add ("not a JSON object");
        return std::nullopt;
    }

    if (root->getProperty (kFormat).toString() != kFormatName)
    {
        problems.add ("not a swingers-map file");
        return std::nullopt;
    }

    int version = 0;
    if (! asInt (root->getProperty (kVersion), version) || version != kFileVersion)
    {
        problems.add ("unsupported map version");
        return std::nullopt;
    }

    DrumMap map;
    map.id = root->getProperty (kId).toString();
    map.name = root->getProperty (kName).toString();
    map.library = root->getProperty (kLibrary).toString();

    if (map.id.isEmpty())
    {
        problems.add ("map has no id");
        return std::nullopt;
    }

    if (map.name.isEmpty())
        map.name = map.id;

    const auto* notes = root->getProperty (kNotes).getArray();
    if (notes == nullptr)
    {
        problems.add ("map has no notes array");
        return std::nullopt;
    }

    for (int i = 0; i < notes->size(); ++i)
    {
        auto* o = (*notes)[i].getDynamicObject();
        int note = 0;
        if (o == nullptr || ! asInt (o->getProperty (kNote), note))
        {
            problems.add ("entry " + juce::String (i) + ": missing or invalid note");
            continue;
        }

        if (note < 0 || note > 127)
        {
            problems.add ("entry " + juce::String (i) + ": note " + juce::String (note) + " out of range");
            continue;
        }

        if (map.find (note) != nullptr)
        {
            problems.add ("entry " + juce::String (i) + ": duplicate note " + juce::String (note));
            continue;
        }

        MapNote n;
        n.note = note;
        n.name = o->getProperty (kName).toString();

        if (o->hasProperty (kGm))
        {
            int gm = 0;
            if (asInt (o->getProperty (kGm), gm) && gm >= 0 && gm <= 127)
                n.gm = gm;
            else
                problems.add ("entry " + juce::String (i) + ": invalid gm voice, treated as none");
        }

        n.preferred = (bool) o->getProperty (kPreferred);
        map.notes.push_back (std::move (n));
    }

    std::sort (map.notes.begin(), map.notes.end(), [] (const MapNote& a, const MapNote& b) { return a.note < b.note; });
    return map;
}
} // namespace swingers
