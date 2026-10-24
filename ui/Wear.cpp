#include "Wear.h"

#include <algorithm>
#include <cmath>

namespace swingers
{
namespace
{
constexpr float kScrewInset = 12.0f;
constexpr float kScrewSize = 11.2f;
constexpr float kTapeEndMargin = 6.0f; // torn ends and glue residue stick out this far horizontally
constexpr float kTapeEdgeMargin = 1.6f;
constexpr float kTapeShadow = 1.5f;

float rnd (juce::Random& r, float lo, float hi) { return lo + r.nextFloat() * (hi - lo); }

bool hitsAny (juce::Rectangle<float> box, const std::vector<juce::Rectangle<int>>& keepClear)
{
    for (const auto& keep : keepClear)
        if (box.intersects (keep.toFloat()))
            return true;
    return false;
}

// Rough width of the handwriting at font size `fontSize` (Homemade Apple: about 0.45 of the font size per character).
float estimateTextWidth (const juce::String& text, float fontSize) { return (float) text.length() * fontSize * 0.45f; }

WearItem makeTape (juce::Random& r, const juce::String& text, float shrink)
{
    const auto fontSize = rnd (r, 8.0f, 9.5f) * shrink;
    const auto w = std::max (38.0f, estimateTextWidth (text, fontSize) + 12.0f);
    const auto h = fontSize * 1.9f;

    WearItem tape;
    tape.kind = WearKind::Tape;
    tape.bounds = { 0.0f, 0.0f, w, h };
    tape.angle = rnd (r, -4.0f, 4.0f) * shrink;
    tape.text = text;
    tape.variant = r.nextInt();
    return tape;
}
} // namespace

const std::vector<juce::String>& wearTapeTexts()
{
    static const std::vector<juce::String> texts { "don't touch!", "kick = 36", "check the hats", "v2 final",
                                                   "ask Marco",    "TODO",      "mine!",          "maybe?" };
    return texts;
}

juce::Rectangle<float> wearExtent (const WearItem& item)
{
    switch (item.kind)
    {
        case WearKind::Scratch:
        {
            // Start, end and the largest bend of the curve (12% of the length).
            const auto a = juce::degreesToRadians (item.angle);
            const auto len = item.bounds.getWidth();
            const juce::Point<float> p0 = item.bounds.getPosition();
            const juce::Point<float> p1 = p0 + juce::Point<float> (std::cos (a), std::sin (a)) * len;
            return juce::Rectangle<float>::leftTopRightBottom (std::min (p0.x, p1.x), std::min (p0.y, p1.y),
                                                               std::max (p0.x, p1.x), std::max (p0.y, p1.y))
                .expanded (len * 0.12f + 2.0f);
        }
        case WearKind::Stain:
            return item.bounds;
        case WearKind::Screw:
            return item.bounds.expanded (2.0f);
        case WearKind::Tape:
        {
            const auto local = juce::Rectangle<float> (item.bounds.getX() - kTapeEndMargin,
                                                       item.bounds.getY() - kTapeEdgeMargin,
                                                       item.bounds.getWidth() + 2.0f * kTapeEndMargin + kTapeShadow,
                                                       item.bounds.getHeight() + 2.0f * kTapeEdgeMargin + kTapeShadow);
            juce::Path p;
            p.addRectangle (local);
            p.applyTransform (juce::AffineTransform::rotation (juce::degreesToRadians (item.angle),
                                                               item.bounds.getCentreX(), item.bounds.getCentreY()));
            return p.getBounds();
        }
    }
    return item.bounds;
}

std::vector<WearItem> generateWear (uint32_t seed, juce::Rectangle<int> area,
                                    const std::vector<juce::Rectangle<int>>& keepClear)
{
    std::vector<WearItem> items;
    if (area.isEmpty())
        return items;

    // One generator per category: stains and tape retry until they clear the keep-clear list, and that must
    // not shift the numbers the other categories draw (screws stay put whatever the controls are).
    const auto generatorFor = [seed] (juce::int64 category) {
        return juce::Random ((juce::int64) seed * 2654435761LL + 12345 + category * 0x2545F4914F6CDD1DLL);
    };
    auto scratchRandom = generatorFor (1);
    auto stainRandom = generatorFor (2);
    auto tapeRandom = generatorFor (3);
    auto screwRandom = generatorFor (4);

    const auto bounds = area.toFloat();
    const auto size = (float) area.getWidth() * (float) area.getHeight();

    // Thin light scratches, mostly along the panel, a few long ones.
    const auto scratches = juce::jlimit (6, 60, (int) (size / 5000.0f));
    for (int i = 0; i < scratches; ++i)
    {
        WearItem s;
        s.kind = WearKind::Scratch;
        const auto len = scratchRandom.nextFloat() < 0.12f ? rnd (scratchRandom, 70.0f, 140.0f) : rnd (scratchRandom, 10.0f, 55.0f);
        s.bounds = { rnd (scratchRandom, bounds.getX() + 4.0f, bounds.getRight() - 4.0f),
                     rnd (scratchRandom, bounds.getY() + 4.0f, bounds.getBottom() - 4.0f), len, 0.0f };
        s.angle = scratchRandom.nextFloat() < 0.6f ? rnd (scratchRandom, -20.0f, 20.0f) : rnd (scratchRandom, 0.0f, 360.0f);
        s.variant = scratchRandom.nextInt();
        items.push_back (s);
    }

    // Opaque dark stains with sharp edges: a few blobs, a few specks.
    const auto stains = juce::jlimit (3, 30, (int) (size / 14000.0f));
    for (int i = 0; i < stains; ++i)
    {
        const bool speck = stainRandom.nextFloat() < 0.4f;
        const auto rx = speck ? rnd (stainRandom, 0.8f, 2.0f) : rnd (stainRandom, 3.0f, 12.0f);
        const auto ry = speck ? rx * rnd (stainRandom, 0.7f, 1.0f) : rnd (stainRandom, 1.8f, std::min (rx, 6.0f));
        for (int attempt = 0; attempt < 40; ++attempt)
        {
            WearItem s;
            s.kind = WearKind::Stain;
            s.bounds = juce::Rectangle<float> (2.0f * rx, 2.0f * ry)
                           .withCentre ({ rnd (stainRandom, bounds.getX() + rx + 3.0f, bounds.getRight() - rx - 3.0f),
                                          rnd (stainRandom, bounds.getY() + ry + 3.0f, bounds.getBottom() - ry - 3.0f) });
            s.variant = stainRandom.nextInt();
            if (! hitsAny (s.bounds.expanded (1.0f), keepClear))
            {
                items.push_back (s);
                break;
            }
        }
    }

    // One to three pieces of masking tape, where they fit.
    const auto wanted = 1 + tapeRandom.nextInt (3);
    const auto& texts = wearTapeTexts();
    std::vector<juce::Rectangle<float>> placed;
    juce::StringArray used; // no text twice on one panel
    const auto inside = bounds.reduced (2.0f);
    for (int t = 0; t < wanted; ++t)
    {
        bool done = false;
        // First with a text from the whole list, then with the short ones only.
        for (int pass = 0; pass < 2 && ! done; ++pass)
        {
            juce::StringArray candidates;
            for (const auto& s : texts)
                if (! used.contains (s) && (pass == 0 || s.length() <= 9))
                    candidates.add (s);
            if (candidates.isEmpty())
                continue;
            const auto text = candidates[tapeRandom.nextInt (candidates.size())];

            for (int attempt = 0; attempt < 300 && ! done; ++attempt)
            {
                // Later attempts use slightly smaller and flatter pieces.
                const auto shrink = 1.0f - 0.25f * (float) attempt / 300.0f;
                auto tape = makeTape (tapeRandom, text, shrink);
                tape.bounds.setPosition (rnd (tapeRandom, inside.getX(), inside.getRight() - tape.bounds.getWidth()),
                                         rnd (tapeRandom, inside.getY(), inside.getBottom() - tape.bounds.getHeight()));

                const auto extent = wearExtent (tape);
                if (! inside.contains (extent) || hitsAny (extent.expanded (1.0f), keepClear))
                    continue;
                const bool overlaps = std::any_of (placed.begin(), placed.end(), [&] (const auto& p) {
                    return p.expanded (3.0f).intersects (extent);
                });
                if (overlaps)
                    continue;

                placed.push_back (extent);
                used.add (text);
                items.push_back (tape);
                done = true;
            }
        }
    }

    // Four screws, a resting angle each; about 6% of the panels lose a screw.
    const int missing = screwRandom.nextFloat() < 0.06f ? screwRandom.nextInt (4) : -1;
    const juce::Point<float> corners[4] = { { bounds.getX() + kScrewInset, bounds.getY() + kScrewInset },
                                            { bounds.getRight() - kScrewInset, bounds.getY() + kScrewInset },
                                            { bounds.getX() + kScrewInset, bounds.getBottom() - kScrewInset },
                                            { bounds.getRight() - kScrewInset, bounds.getBottom() - kScrewInset } };
    for (int i = 0; i < 4; ++i)
    {
        WearItem s;
        s.kind = WearKind::Screw;
        s.bounds = juce::Rectangle<float> (kScrewSize, kScrewSize).withCentre (corners[i]);
        s.angle = rnd (screwRandom, 0.0f, 180.0f);
        s.variant = i == missing ? 2 : (screwRandom.nextFloat() < 0.5f ? 0 : 1);
        items.push_back (s);
    }

    return items;
}
} // namespace swingers
