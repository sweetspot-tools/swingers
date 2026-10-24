#include "PluginState.h"

namespace swingers
{
namespace
{
constexpr const char* kFormat = "swingers-state";
constexpr int kVersion = 1;

std::optional<DrumMap> mapCopyFrom (const juce::var& value)
{
    if (! value.isObject())
        return std::nullopt;
    juce::StringArray problems;
    return drumMapFromJson (value, problems);
}

bool sameContent (const juce::var& a, const juce::var& b)
{
    return juce::JSON::toString (a, true) == juce::JSON::toString (b, true);
}

juce::String displayName (const DrumMap& map)
{
    return map.name.isNotEmpty() ? map.name : map.id;
}

DrumMap resolveMap (const juce::String& id, const std::optional<DrumMap>& copy, const MapLibrary& library,
                    juce::StringArray& warnings)
{
    const bool haveCopy = copy && copy->id == id;

    if (const auto* found = library.find (id))
    {
        // Ids are only unique per computer: a different map under the same id must not change the project.
        if (haveCopy && ! sameContent (toJson (*found), toJson (*copy)))
        {
            warnings.add ("Map '" + displayName (*copy)
                          + "' on this computer differs from the copy saved in the project: using the project's copy");
            return *copy;
        }
        return *found;
    }

    if (haveCopy)
    {
        warnings.add ("Map '" + displayName (*copy) + "' not found on this computer: using the copy saved in the project");
        return *copy;
    }

    warnings.add ("Map '" + id + "' not found and no copy in the project: using General MIDI");

    if (const auto* gm = library.find (kGeneralMidiId))
        return *gm;

    DrumMap fallback;
    fallback.id = kGeneralMidiId;
    fallback.name = "General MIDI";
    return fallback;
}
} // namespace

juce::MemoryBlock serialize (const PluginState& state)
{
    auto* root = new juce::DynamicObject();
    juce::var json (root);
    root->setProperty ("format", kFormat);
    root->setProperty ("version", kVersion);
    root->setProperty ("from", state.fromId);
    root->setProperty ("to", state.toId);
    root->setProperty ("channel", state.channel);
    if (state.wearSeed != 0)
        root->setProperty ("wearSeed", (juce::int64) state.wearSeed);
    if (state.fromCopy)
        root->setProperty ("fromMap", toJson (*state.fromCopy));
    if (state.toCopy)
        root->setProperty ("toMap", toJson (*state.toCopy));
    if (state.overridesCopy)
        root->setProperty ("overrides", toJson (*state.overridesCopy));

    const auto text = juce::JSON::toString (json, true);
    return { text.toRawUTF8(), text.getNumBytesAsUTF8() };
}

std::optional<PluginState> deserialize (const void* data, size_t size)
{
    if (data == nullptr || size == 0)
        return std::nullopt;

    juce::var json;
    if (! juce::JSON::parse (juce::String::fromUTF8 (static_cast<const char*> (data), (int) size), json).wasOk()
        || ! json.isObject())
        return std::nullopt;

    if (json["format"].toString() != kFormat || ! json["version"].isInt() || (int) json["version"] != kVersion)
        return std::nullopt;

    PluginState state;
    if (const auto from = json["from"].toString(); from.isNotEmpty())
        state.fromId = from;
    if (const auto to = json["to"].toString(); to.isNotEmpty())
        state.toId = to;
    state.channel = juce::jlimit (0, 16, (int) json["channel"]);
    state.wearSeed = (uint32_t) juce::jlimit<juce::int64> (0, 0xffffffffLL, (juce::int64) json["wearSeed"]);
    state.fromCopy = mapCopyFrom (json["fromMap"]);
    state.toCopy = mapCopyFrom (json["toMap"]);

    if (json["overrides"].isObject())
    {
        juce::StringArray problems;
        state.overridesCopy = pairOverridesFromJson (json["overrides"], problems);
    }

    return state;
}

ResolvedMaps resolve (const PluginState& state, const MapLibrary& library)
{
    ResolvedMaps out;
    out.from = resolveMap (state.fromId, state.fromCopy, library, out.warnings);
    out.to = resolveMap (state.toId, state.toCopy, library, out.warnings);

    out.overrides = library.overrides (out.from.id, out.to.id);
    if (state.overridesCopy && state.overridesCopy->fromId == out.from.id && state.overridesCopy->toId == out.to.id)
    {
        const auto& copy = *state.overridesCopy;
        if (out.overrides.empty())
        {
            out.overrides = copy; // no pair file here: the project's copy, silently
        }
        else if (! sameContent (toJson (out.overrides), toJson (copy)))
        {
            out.warnings.add ("Exceptions for '" + displayName (out.from) + "' to '" + displayName (out.to)
                              + "' on this computer differ from the copy saved in the project: using the project's copy");
            out.overrides = copy;
        }
    }

    return out;
}
} // namespace swingers
