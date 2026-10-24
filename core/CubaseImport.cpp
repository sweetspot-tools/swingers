#include "CubaseImport.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

namespace swingers
{
namespace
{
// All characters examined here are ASCII, and UTF-8 multi-byte sequences never contain ASCII bytes,
// so the pre-pass works on raw bytes: one linear walk, no String indexing.
bool startsWith (const std::string& s, size_t pos, const char* prefix)
{
    return s.compare (pos, std::strlen (prefix), prefix) == 0;
}

bool startsEntity (const std::string& s, size_t pos)
{
    static const char* const entities[] = { "&lt;", "&gt;", "&amp;", "&quot;", "&apos;" };
    for (auto* e : entities)
        if (startsWith (s, pos, e))
            return true;

    if (pos + 1 >= s.size() || s[pos + 1] != '#')
        return false;

    auto i = pos + 2;
    const bool hex = i < s.size() && (s[i] == 'x' || s[i] == 'X');
    if (hex)
        ++i;

    const auto digitsStart = i;
    while (i < s.size() && (hex ? std::isxdigit ((unsigned char) s[i]) : std::isdigit ((unsigned char) s[i])))
        ++i;

    return i > digitsStart && i < s.size() && s[i] == ';';
}

// Cubase writes names such as `[Ride ><] *Ride Tip` or `Bell & Tip` without escaping them. Inside double-quoted
// attribute values, escape `<`, `>` and any `&` that does not already open an entity. Comments are left alone.
juce::String escapeAttributeValues (const juce::String& text)
{
    const std::string in (text.toRawUTF8());
    std::string out;
    out.reserve (in.size() + in.size() / 8 + 64);

    bool inTag = false, inValue = false;

    for (size_t i = 0; i < in.size(); ++i)
    {
        const char c = in[i];

        if (! inTag)
        {
            if (c == '<')
            {
                if (startsWith (in, i, "<!--"))
                {
                    const auto end = in.find ("-->", i + 4);
                    const auto stop = end == std::string::npos ? in.size() : end + 3;
                    out.append (in, i, stop - i);
                    i = stop - 1;
                    continue;
                }
                inTag = true;
            }
            out += c;
            continue;
        }

        if (inValue)
        {
            if (c == '"')
            {
                inValue = false;
                out += c;
            }
            else if (c == '<')
                out += "&lt;";
            else if (c == '>')
                out += "&gt;";
            else if (c == '&' && ! startsEntity (in, i))
                out += "&amp;";
            else
                out += c;
            continue;
        }

        if (c == '"')
            inValue = true;
        else if (c == '>')
            inTag = false;

        out += c;
    }

    return juce::String::fromUTF8 (out.data(), (int) out.size());
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
    if (! childValue (item, "int", key, v))
        return false;

    v = v.trim();
    if (v.isEmpty() || v.length() > 3 || ! v.containsOnly ("0123456789"))
        return false;

    note = v.getIntValue();
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
        {
            list = e;
            break;
        }

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
