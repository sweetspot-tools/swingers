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

DrumMap resolveMap (const juce::String& id, const std::optional<DrumMap>& copy, const MapLibrary& library,
                    juce::StringArray& warnings)
{
    if (const auto* found = library.find (id))
        return *found;

    if (copy && copy->id == id)
    {
        warnings.add ("Map '" + copy->name + "' not found on this computer: using the copy saved in the project");
        return *copy;
    }

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
    if (out.overrides.empty() && state.overridesCopy && state.overridesCopy->fromId == out.from.id
        && state.overridesCopy->toId == out.to.id)
        out.overrides = *state.overridesCopy;

    return out;
}
} // namespace swingers
