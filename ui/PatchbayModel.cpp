#include "PatchbayModel.h"

#include <algorithm>

namespace swingers
{
PatchbayModel PatchbayModel::build (const DrumMap& from, const DrumMap& to, const ConversionTable& table,
                                    bool allNotes)
{
    PatchbayModel model;
    model.showGmColumn = ! from.isGeneralMidi() && ! to.isGeneralMidi();

    const auto stateOf = [&] (int note) {
        switch (table.entries[(size_t) note].route)
        {
            case Route::Standard:
            case Route::Override:     return SourceState::Connected;
            case Route::DroppedByUser: return SourceState::DroppedByUser;
            case Route::NoMatch:      break;
        }
        return SourceState::NoMatch;
    };

    if (allNotes)
    {
        for (int note = 0; note < 128; ++note)
        {
            const auto* f = from.find (note);
            model.sources.push_back ({ note, f != nullptr ? f->name : juce::String(), stateOf (note) });
            const auto* t = to.find (note);
            model.targets.push_back (t != nullptr ? *t : MapNote { note, {}, kNoGm, false });
        }
    }
    else
    {
        for (const auto& n : from.notes)
            if (n.note >= 0 && n.note < 128)
                model.sources.push_back ({ n.note, n.name, stateOf (n.note) });
        for (const auto& n : to.notes)
            if (n.note >= 0 && n.note < 128)
                model.targets.push_back (n);
    }

    for (const auto& row : model.sources)
    {
        if (row.state != SourceState::Connected)
            continue;
        const auto& e = table.entries[(size_t) row.note];
        const bool direct = e.route == Route::Override;
        model.cables.push_back ({ row.note, direct ? kNoGm : e.gm, e.target, direct });
        if (! direct && model.showGmColumn && e.gm != kNoGm)
            model.gmVoices.push_back (e.gm);
    }

    std::sort (model.gmVoices.begin(), model.gmVoices.end());
    model.gmVoices.erase (std::unique (model.gmVoices.begin(), model.gmVoices.end()), model.gmVoices.end());
    return model;
}

const PatchRow* PatchbayModel::findSource (int note) const
{
    const auto it = std::find_if (sources.begin(), sources.end(), [note] (const auto& r) { return r.note == note; });
    return it != sources.end() ? &*it : nullptr;
}

int PatchbayModel::targetIndexOf (int note) const
{
    const auto it = std::find_if (targets.begin(), targets.end(), [note] (const auto& t) { return t.note == note; });
    return it != targets.end() ? (int) (it - targets.begin()) : -1;
}
} // namespace swingers
