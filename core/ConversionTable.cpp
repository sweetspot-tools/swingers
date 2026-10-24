#include "ConversionTable.h"

namespace swingers
{
ConversionTable ConversionTable::build (const DrumMap& from, const DrumMap& to, const PairOverrides* overrides)
{
    // Target note for each GM voice; `notes` is sorted, so the first hit is the lowest.
    std::array<int, 128> lowest, lowestPreferred;
    lowest.fill (-1);
    lowestPreferred.fill (-1);

    for (const auto& n : to.notes)
    {
        if (n.gm < 0 || n.gm > 127 || n.note < 0 || n.note > 127)
            continue;

        auto& low = lowest[(size_t) n.gm];
        if (low < 0 || n.note < low)
            low = n.note;

        if (n.preferred)
        {
            auto& pref = lowestPreferred[(size_t) n.gm];
            if (pref < 0 || n.note < pref)
                pref = n.note;
        }
    }

    ConversionTable table;

    for (int note = 0; note < 128; ++note)
    {
        auto& e = table.entries[(size_t) note];
        e.gm = from.gmVoiceOf (note);

        if (overrides != nullptr)
        {
            const auto direct = overrides->direct.find (note);
            if (direct != overrides->direct.end())
            {
                e.target = direct->second;
                e.route = Route::Override;
                continue;
            }

            if (overrides->dropped.count (note) != 0)
            {
                e.route = Route::DroppedByUser;
                continue;
            }
        }

        if (e.gm >= 0 && e.gm <= 127)
        {
            const int target = lowestPreferred[(size_t) e.gm] >= 0 ? lowestPreferred[(size_t) e.gm] : lowest[(size_t) e.gm];
            if (target >= 0)
            {
                e.target = target;
                e.route = Route::Standard;
            }
        }
    }

    return table;
}

ConversionTable ConversionTable::identity()
{
    ConversionTable table;
    for (int note = 0; note < 128; ++note)
        table.entries[(size_t) note] = { note, Route::Standard, note };

    return table;
}
} // namespace swingers
