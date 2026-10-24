#pragma once

#include <juce_core/juce_core.h>

#include <map>
#include <optional>
#include <set>

namespace swingers
{
// Per-pair exceptions to the standard GM-pivot conversion, for one (from, to) map pair.
struct PairOverrides
{
    juce::String fromId, toId;
    std::map<int, int> direct; // source note -> target note
    std::set<int> dropped;     // source notes discarded on purpose

    bool empty() const;
    void connect (int source, int target); // removes `source` from dropped
    void drop (int source);                // removes `source` from direct
    void reset (int source);               // removes it from both
};

juce::var toJson (const PairOverrides&);

// Returns nullopt when the document is not a version 1 "swingers-pair".
// Bad entries (out of range, malformed, duplicate) are skipped and described in `problems`;
// a note both in `direct` and `dropped` keeps the `direct` entry and is reported.
std::optional<PairOverrides> pairOverridesFromJson (const juce::var&, juce::StringArray& problems);
} // namespace swingers
