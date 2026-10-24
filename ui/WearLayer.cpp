#include "WearLayer.h"

#include "BinaryData.h"
#include "RackLookAndFeel.h"

#include <cmath>

namespace swingers
{
namespace
{
const juce::Colour kPaper { 0xffe6dab6 };
const juce::Colour kPaperFibre { 0xff8f8560 };
const juce::Colour kGlue { 0xffb8a66b };
const juce::Colour kInk { 0xff2b2620 };

float rnd (juce::Random& r, float lo, float hi) { return lo + r.nextFloat() * (hi - lo); }

float textWidth (const juce::Font& font, const juce::String& text)
{
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (font, text, 0.0f, 0.0f);
    return glyphs.getBoundingBox (0, -1, true).getWidth();
}

// Panel grain: 256 x 256 tile of faint light and dark dots, the same for every panel.
juce::Image makeGrainTile()
{
    constexpr int kTile = 256;
    juce::Image tile (juce::Image::ARGB, kTile, kTile, true);
    juce::Image::BitmapData data (tile, juce::Image::BitmapData::writeOnly);
    juce::Random r (0x5eed);
    for (int y = 0; y < kTile; ++y)
    {
        for (int x = 0; x < kTile; ++x)
        {
            const auto v = r.nextInt (100);
            if (v < 30)
                data.setPixelColour (x, y, juce::Colour::fromRGBA (255, 255, 255, (juce::uint8) (2 + r.nextInt (7))));
            else if (v < 65)
                data.setPixelColour (x, y, juce::Colour::fromRGBA (0, 0, 0, (juce::uint8) (3 + r.nextInt (10))));
        }
    }
    return tile;
}

void drawScratch (juce::Graphics& g, const WearItem& item)
{
    juce::Random r (item.variant);
    const auto a = juce::degreesToRadians (item.angle);
    const auto len = item.bounds.getWidth();
    const auto p0 = item.bounds.getPosition();
    const auto p1 = p0 + juce::Point<float> (std::cos (a), std::sin (a)) * len;
    const auto bend = rnd (r, -0.12f, 0.12f) * len;
    const auto mid = (p0 + p1) * 0.5f + juce::Point<float> (std::sin (a), -std::cos (a)) * bend;
    const auto width = rnd (r, 0.35f, 0.8f);

    juce::Path path;
    path.startNewSubPath (p0);
    path.quadraticTo (mid, p1);

    // Dark lip under the light line, as in the preview.
    g.setColour (juce::Colours::black.withAlpha (rnd (r, 0.3f, 0.5f)));
    g.strokePath (path, juce::PathStrokeType (width), juce::AffineTransform::translation (0.5f, 0.6f));
    g.setColour (juce::Colour (0xffd6d6d0).withAlpha (rnd (r, 0.06f, 0.2f)));
    g.strokePath (path, juce::PathStrokeType (width));
}

// Closed irregular outline inside `box`, sharp-edged.
juce::Path blobPath (juce::Random& r, juce::Rectangle<float> box, int points)
{
    juce::Path p;
    const auto c = box.getCentre();
    for (int i = 0; i < points; ++i)
    {
        const auto a = juce::MathConstants<float>::twoPi * (float) i / (float) points;
        const auto k = rnd (r, 0.55f, 1.0f);
        const juce::Point<float> pt (c.x + std::cos (a) * box.getWidth() * 0.5f * k,
                                     c.y + std::sin (a) * box.getHeight() * 0.5f * k);
        if (i == 0)
            p.startNewSubPath (pt);
        else
            p.lineTo (pt);
    }
    p.closeSubPath();
    return p;
}

void drawStain (juce::Graphics& g, const WearItem& item)
{
    juce::Random r (item.variant);
    g.setColour (r.nextBool() ? juce::Colour (0xff0c0a07) : juce::Colour (0xff12100b));
    g.fillPath (blobPath (r, item.bounds, item.bounds.getWidth() < 5.0f ? 6 : 10));
}

void drawScrew (juce::Graphics& g, const WearItem& item)
{
    const auto c = item.bounds.getCentre();
    if (item.variant == 2) // missing: only the hole is left
    {
        g.setColour (juce::Colour (0xff060606));
        g.fillEllipse (juce::Rectangle<float> (10.4f, 10.4f).withCentre (c));
        g.setColour (juce::Colour (0xff2e2e31));
        g.drawEllipse (juce::Rectangle<float> (10.4f, 10.4f).withCentre (c), 1.2f);
        g.setColour (juce::Colours::black);
        g.fillEllipse (juce::Rectangle<float> (7.2f, 7.2f).withCentre (c.translated (0.0f, 0.8f)));
        return;
    }

    g.setColour (juce::Colour (0xff2b2b2e));
    g.fillEllipse (item.bounds);
    g.setColour (juce::Colour (0xff4c4c50));
    g.drawEllipse (item.bounds, 1.0f);
    g.setColour (juce::Colour (0xff202022).withAlpha (0.7f));
    g.fillEllipse (juce::Rectangle<float> (9.2f, 9.2f).withCentre (c));

    g.saveState();
    g.addTransform (juce::AffineTransform::rotation (juce::degreesToRadians (item.angle), c.x, c.y));
    g.setColour (juce::Colour (0xff050505));
    g.fillRoundedRectangle (juce::Rectangle<float> (item.variant == 0 ? 10.0f : 9.6f, 2.0f).withCentre (c), 0.6f);
    if (item.variant == 1)
        g.fillRoundedRectangle (juce::Rectangle<float> (2.0f, 9.6f).withCentre (c), 0.6f);
    g.restoreState();
}

// Torn strip outline in the tape's own coordinates (origin = top-left of the item bounds).
juce::Path tapeOutline (juce::Random& r, float w, float h)
{
    std::vector<juce::Point<float>> pts;
    pts.push_back ({ 0.0f, rnd (r, 0.0f, 2.0f) });
    for (int k = 1; k <= 4; ++k)
        pts.push_back ({ w * (float) k / 4.0f, rnd (r, -1.2f, 1.2f) });
    for (int q = 1; q <= 7; ++q) // torn right end
        pts.push_back ({ w + rnd (r, -3.0f, 3.0f), h * (float) q / 7.0f });
    for (int k = 3; k >= 0; --k)
        pts.push_back ({ w * (float) k / 4.0f, h + rnd (r, -1.2f, 1.2f) });
    for (int q = 6; q >= 1; --q) // torn left end
        pts.push_back ({ rnd (r, -3.0f, 3.0f), h * (float) q / 7.0f });

    juce::Path p;
    for (size_t i = 0; i < pts.size(); ++i)
        i == 0 ? p.startNewSubPath (pts[i]) : p.lineTo (pts[i]);
    p.closeSubPath();
    return p;
}

void drawTapeBody (juce::Graphics& g, const WearItem& item)
{
    juce::Random r (item.variant);
    const auto w = item.bounds.getWidth();
    const auto h = item.bounds.getHeight();
    const auto outline = tapeOutline (r, w, h);

    g.saveState();
    g.addTransform (juce::AffineTransform::translation (item.bounds.getX(), item.bounds.getY())
                        .rotated (juce::degreesToRadians (item.angle), item.bounds.getCentreX(),
                                  item.bounds.getCentreY()));

    // Hard shadow (no blur), then the paper, then fibres.
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillPath (outline, juce::AffineTransform::translation (0.8f, 1.2f));
    g.setColour (kPaper.withAlpha (0.93f));
    g.fillPath (outline);

    g.setColour (kPaperFibre.withAlpha (0.16f));
    for (int i = 0; i < 5; ++i)
        g.drawLine (rnd (r, 0.0f, w * 0.3f), rnd (r, 2.0f, h - 2.0f), rnd (r, w * 0.6f, w), rnd (r, 2.0f, h - 2.0f), 0.5f);

    // Glue residue past both ends: small dull-yellow flecks.
    g.setColour (kGlue.withAlpha (0.35f));
    for (int end = 0; end < 2; ++end)
    {
        for (int i = 0; i < 3; ++i)
        {
            const auto x = end == 0 ? rnd (r, -5.0f, -2.5f) : w + rnd (r, 2.5f, 5.0f);
            const auto rad = rnd (r, 0.7f, 1.6f);
            g.fillPath (blobPath (r, juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre ({ x, rnd (r, 1.0f, h - 1.0f) }), 6));
        }
    }
    g.restoreState();
}

void drawTapeText (juce::Graphics& g, const WearItem& item, const juce::Typeface::Ptr& typeface)
{
    if (typeface == nullptr || item.text.isEmpty())
        return;

    juce::Random r (item.variant ^ 0x7a9e);
    const auto w = item.bounds.getWidth();
    const auto h = item.bounds.getHeight();

    // Largest size that fits the strip, measured for the whole line.
    auto font = juce::Font (juce::FontOptions (typeface)).withHeight (h * 1.05f);
    const auto avail = w - 10.0f;
    const auto measured = textWidth (font, item.text);
    if (measured > avail)
        font = font.withHeight (font.getHeight() * avail / measured);

    g.saveState();
    g.addTransform (juce::AffineTransform::translation (item.bounds.getX(), item.bounds.getY())
                        .rotated (juce::degreesToRadians (item.angle), item.bounds.getCentreX(),
                                  item.bounds.getCentreY()));
    g.addTransform (juce::AffineTransform::rotation (juce::degreesToRadians (rnd (r, -2.5f, 2.5f)), w * 0.5f, h * 0.5f));

    auto x = (w - std::min (avail, measured)) * 0.5f;
    const auto baseline = h * 0.68f;
    g.setColour (kInk.withAlpha (0.95f));
    for (const auto ch : item.text)
    {
        const auto glyph = juce::String::charToString (ch);
        const auto scale = rnd (r, 0.94f, 1.06f);
        const auto letter = font.withHeight (font.getHeight() * scale);
        const auto dy = rnd (r, -0.7f, 0.7f);
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (juce::degreesToRadians (rnd (r, -4.0f, 4.0f)))
                            .translated (x, baseline + dy));
        g.setFont (letter);
        g.drawSingleLineText (glyph, 0, 0);
        g.restoreState();
        x += textWidth (font, glyph);
    }
    g.restoreState();
}
} // namespace

WearLayer::WearLayer (Layer l) : layer (l)
{
    setInterceptsMouseClicks (false, false);
    setOpaque (layer == Layer::Background);
}

void WearLayer::setSeed (uint32_t newSeed)
{
    if (seed == newSeed)
        return;
    seed = newSeed;
    dirty = true;
    repaint();
}

void WearLayer::setKeepClear (std::vector<juce::Rectangle<int>> rectangles)
{
    if (keepClear == rectangles)
        return;
    keepClear = std::move (rectangles);
    dirty = true;
    repaint();
}

void WearLayer::resized()
{
    dirty = true;
}

void WearLayer::rebuild (float scale)
{
    dirty = false;
    cachedScale = scale;
    const auto w = getWidth(), h = getHeight();
    cache = juce::Image (juce::Image::ARGB, juce::jmax (1, (int) std::ceil ((float) w * scale)),
                         juce::jmax (1, (int) std::ceil ((float) h * scale)), true);
    if (w <= 0 || h <= 0)
        return;

    juce::Graphics g (cache);
    g.addTransform (juce::AffineTransform::scale (scale));
    const auto items = generateWear (seed, getLocalBounds(), keepClear);

    if (layer == Layer::Background)
    {
        g.fillAll (colours::panel);
        g.saveState();
        g.addTransform (juce::AffineTransform::scale (1.0f / scale));
        g.setTiledImageFill (makeGrainTile(), 0, 0, 1.0f);
        g.fillRect (juce::Rectangle<int> (0, 0, cache.getWidth(), cache.getHeight()));
        g.restoreState();

        for (const auto& item : items)
            if (item.kind == WearKind::Scratch)
                drawScratch (g, item);
        for (const auto& item : items)
            if (item.kind == WearKind::Stain)
                drawStain (g, item);
        for (const auto& item : items)
            if (item.kind == WearKind::Screw)
                drawScrew (g, item);
        return;
    }

    if (tapeTypeface == nullptr)
        tapeTypeface = juce::Typeface::createSystemTypefaceFor (BinaryData::HomemadeAppleRegular_ttf,
                                                                (size_t) BinaryData::HomemadeAppleRegular_ttfSize);
    for (const auto& item : items)
        if (item.kind == WearKind::Tape)
            drawTapeBody (g, item);
    // All handwriting above all paper.
    for (const auto& item : items)
        if (item.kind == WearKind::Tape)
            drawTapeText (g, item, tapeTypeface);
}

void WearLayer::paint (juce::Graphics& g)
{
    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    const auto scale = juce::jlimit (1.0f, 4.0f, std::ceil (g.getInternalContext().getPhysicalPixelScaleFactor()));
    if (dirty || ! juce::approximatelyEqual (scale, cachedScale) || ! cache.isValid())
        rebuild (scale);

    g.drawImageTransformed (cache, juce::AffineTransform::scale (1.0f / scale));
}
} // namespace swingers
