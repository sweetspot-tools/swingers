#include "CubaseImport.h"

#include <algorithm>

namespace swingers
{
namespace
{
bool startsEntity (const juce::String& text, int pos)
{
    static const char* const entities[] = { "&lt;", "&gt;", "&amp;", "&quot;", "&apos;" };
    for (auto* e : entities)
        if (text.substring (pos).startsWith (e))
            return true;

    if (text[pos + 1] != '#')
        return false;

    auto i = pos + 2;
    if (text[i] == 'x' || text[i] == 'X')
        ++i;

    const auto digitsStart = i;
    while (juce::CharacterFunctions::getHexDigitValue (text[i]) >= 0)
        ++i;

    return i > digitsStart && text[i] == ';';
}

// Cubase writes names such as `[Ride ><] *Ride Tip` or `Bell & Tip` without escaping them. Inside double-quoted
// attribute values, escape `<`, `>` and any `&` that does not already open an entity. Comments are left alone.
juce::String escapeAttributeValues (const juce::String& text)
{
    juce::String out;
    out.preallocateBytes ((size_t) text.length() + 64);

    bool inTag = false, inValue = false;
    const auto length = text.length();

    for (int i = 0; i < length; ++i)
    {
        const auto c = text[i];

        if (! inTag)
        {
            if (c == '<')
            {
                if (text.substring (i).startsWith ("<!--"))
                {
                    const auto end = text.indexOf (i + 4, "-->");
                    const auto stop = end < 0 ? length : end + 3;
                    out << text.substring (i, stop);
                    i = stop - 1;
                    continue;
                }
                inTag = true;
            }
            out << c;
            continue;
        }

        if (inValue)
        {
            if (c == '"')
                inValue = false, out << c;
            else if (c == '<')
                out << "&lt;";
            else if (c == '>')
                out << "&gt;";
            else if (c == '&' && ! startsEntity (text, i))
                out << "&amp;";
            else
                out << c;
            continue;
        }

        if (c == '"')
            inValue = true;
        else if (c == '>')
            inTag = false;

        out << c;
    }

    return out;
}

juce::String slugOf (const juce::String& name)
{
    juce::String slug;
    for (auto c : name.toLowerCase())
    {
        const bool alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        if (alnum)
            slug << c;
        else if (slug.isNotEmpty() && ! slug.endsWithChar ('-'))
            slug << '-';
    }
    return slug.trimCharactersAtEnd ("-");
}

// Value of the direct child `<tag name="key" value="..."/>`, empty if absent.
bool childValue (const juce::XmlElement& parent, const char* tag, const char* key, juce::String& value)
{
    for (auto* e : parent.getChildWithTagNameIterator (tag))
        if (e->getStringAttribute ("name") == key)
        {
            value = e->getStringAttribute ("value");
            return true;
        }
    return false;
}

bool noteChild (const juce::XmlElement& item, const char* key, int& note)
{
    juce::String v;
    if (! childValue (item, "int", key, v) || ! v.trim().containsOnly ("0123456789") || v.trim().isEmpty())
        return false;

    note = v.trim().getIntValue();
    return note >= 0 && note <= 127;
}
} // namespace

std::optional<DrumMap> importCubaseDrumMap (const juce::String& xmlText, juce::String& error)
{
    const auto doc = juce::XmlDocument::parse (escapeAttributeValues (xmlText));
    if (doc == nullptr)
    {
        error = "The file is not valid XML.";
        return std::nullopt;
    }

    if (! doc->hasTagName ("DrumMap"))
    {
        error = "The file is not a Cubase drum map.";
        return std::nullopt;
    }

    juce::String name;
    childValue (*doc, "string", "Name", name);
    name = name.trim();
    if (name.isEmpty())
    {
        error = "The drum map has no name.";
        return std::nullopt;
    }

    const juce::XmlElement* list = nullptr;
    for (auto* e : doc->getChildWithTagNameIterator ("list"))
        if (e->getStringAttribute ("name") == "Map")
            list = e;

    if (list == nullptr)
    {
        error = "The drum map has no Map list.";
        return std::nullopt;
    }

    DrumMap map;
    map.name = name;
    map.library = name;
    map.id = slugOf (name);
    if (map.id.isEmpty())
        map.id = "drum-map";

    for (auto* item : list->getChildWithTagNameIterator ("item"))
    {
        int input = 0, output = 0;
        if (! noteChild (*item, "INote", input) || ! noteChild (*item, "ONote", output))
            continue;

        juce::String voice;
        childValue (*item, "string", "Name", voice);
        voice = voice.trim();
        if (voice.isEmpty() || voice == "---")
            continue;

        if (map.find (input) != nullptr)
            continue; // first wins

        MapNote n;
        n.note = input;
        n.name = voice;
        n.gm = output == 0 ? kNoGm : output;
        map.notes.push_back (n);
    }

    std::sort (map.notes.begin(), map.notes.end(), [] (const MapNote& a, const MapNote& b) { return a.note < b.note; });

    if (map.notes.empty())
    {
        error = "The drum map has no usable voices.";
        return std::nullopt;
    }

    return map;
}
} // namespace swingers
