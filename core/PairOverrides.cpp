#include "PairOverrides.h"

#include <cmath>

namespace swingers
{
namespace
{
const juce::Identifier kFormat { "format" }, kVersion { "version" }, kFrom { "from" }, kTo { "to" },
    kDirect { "direct" }, kDropped { "dropped" }, kNote { "note" };

constexpr auto kFormatName = "swingers-pair";
constexpr int kFileVersion = 1;

bool asInt (const juce::var& v, int& out)
{
    if (! (v.isInt() || v.isInt64() || v.isDouble()))
        return false;

    const double d = v;
    if (! juce::exactlyEqual (d, std::floor (d)) || d < -1.0e9 || d > 1.0e9)
        return false;

    out = (int) d;
    return true;
}

bool inRange (int note)
{
    return note >= 0 && note <= 127;
}
} // namespace

bool PairOverrides::empty() const
{
    return direct.empty() && dropped.empty();
}

void PairOverrides::connect (int source, int target)
{
    dropped.erase (source);
    direct[source] = target;
}

void PairOverrides::drop (int source)
{
    direct.erase (source);
    dropped.insert (source);
}

void PairOverrides::reset (int source)
{
    direct.erase (source);
    dropped.erase (source);
}

juce::var toJson (const PairOverrides& overrides)
{
    juce::Array<juce::var> direct;
    for (const auto& [source, target] : overrides.direct)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty (kNote, source);
        o->setProperty (kTo, target);
        direct.add (juce::var (o));
    }

    juce::Array<juce::var> dropped;
    for (const int source : overrides.dropped)
        dropped.add (source);

    auto* root = new juce::DynamicObject();
    root->setProperty (kFormat, kFormatName);
    root->setProperty (kVersion, kFileVersion);
    root->setProperty (kFrom, overrides.fromId);
    root->setProperty (kTo, overrides.toId);
    root->setProperty (kDirect, direct);
    root->setProperty (kDropped, dropped);
    return juce::var (root);
}

std::optional<PairOverrides> pairOverridesFromJson (const juce::var& json, juce::StringArray& problems)
{
    auto* root = json.getDynamicObject();
    if (root == nullptr)
    {
        problems.add ("not a JSON object");
        return std::nullopt;
    }

    if (root->getProperty (kFormat).toString() != kFormatName)
    {
        problems.add ("not a swingers-pair file");
        return std::nullopt;
    }

    int version = 0;
    if (! asInt (root->getProperty (kVersion), version) || version != kFileVersion)
    {
        problems.add ("unsupported pair version");
        return std::nullopt;
    }

    PairOverrides result;
    result.fromId = root->getProperty (kFrom).toString();
    result.toId = root->getProperty (kTo).toString();

    if (result.fromId.isEmpty() || result.toId.isEmpty())
    {
        problems.add ("pair has no from or to id");
        return std::nullopt;
    }

    if (const auto* direct = root->getProperty (kDirect).getArray())
    {
        for (int i = 0; i < direct->size(); ++i)
        {
            auto* o = (*direct)[i].getDynamicObject();
            int note = 0, target = 0;
            const auto prefix = "direct " + juce::String (i) + ": ";

            if (o == nullptr || ! asInt (o->getProperty (kNote), note) || ! asInt (o->getProperty (kTo), target))
            {
                problems.add (prefix + "missing or invalid note or target");
                continue;
            }

            if (! inRange (note))
            {
                problems.add (prefix + "note " + juce::String (note) + " out of range");
                continue;
            }

            if (! inRange (target))
            {
                problems.add (prefix + "target " + juce::String (target) + " out of range");
                continue;
            }

            if (result.direct.count (note) != 0)
            {
                problems.add (prefix + "duplicate note " + juce::String (note));
                continue;
            }

            result.direct[note] = target;
        }
    }

    if (const auto* dropped = root->getProperty (kDropped).getArray())
    {
        for (int i = 0; i < dropped->size(); ++i)
        {
            int note = 0;
            const auto prefix = "dropped " + juce::String (i) + ": ";

            if (! asInt ((*dropped)[i], note))
            {
                problems.add (prefix + "invalid note");
                continue;
            }

            if (! inRange (note))
            {
                problems.add (prefix + "note " + juce::String (note) + " out of range");
                continue;
            }

            if (result.direct.count (note) != 0)
            {
                problems.add (prefix + "note " + juce::String (note) + " is also connected, keeping the connection");
                continue;
            }

            result.dropped.insert (note);
        }
    }

    return result;
}
} // namespace swingers
