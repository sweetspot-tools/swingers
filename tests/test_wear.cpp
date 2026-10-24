#include <catch2/catch_test_macros.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include "CompactPanel.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "PluginState.h"
#include "Wear.h"
#include "WearLayer.h"

using namespace swingers;

namespace
{
const juce::Rectangle<int> kArea (0, 0, 520, 140);

// The controls of the compact panel, roughly.
const std::vector<juce::Rectangle<int>> kKeepClear {
    { 158, 22, 124, 24 }, { 320, 22, 124, 24 }, { 150, 78, 302, 26 }, { 458, 10, 48, 66 }, { 31, 77, 10, 10 },
    { 12, 113, 496, 16 }, { 26, 10, 120, 37 }, { 16, 60, 40, 14 }
};

int count (const std::vector<WearItem>& items, WearKind kind)
{
    int n = 0;
    for (const auto& i : items)
        n += i.kind == kind ? 1 : 0;
    return n;
}

// Independent of wearExtent(): the rotated strip itself, widened by one pixel.
juce::Rectangle<float> rotatedBounds (const WearItem& item)
{
    juce::Path p;
    p.addRectangle (item.bounds);
    p.applyTransform (juce::AffineTransform::rotation (juce::degreesToRadians (item.angle),
                                                       item.bounds.getCentreX(), item.bounds.getCentreY()));
    return p.getBounds().expanded (1.0f);
}
} // namespace

TEST_CASE ("wear: same seed gives the same items, different seeds differ")
{
    const auto a = generateWear (12345, kArea, kKeepClear);
    const auto b = generateWear (12345, kArea, kKeepClear);
    REQUIRE_FALSE (a.empty());
    CHECK (a == b);

    int different = 0;
    for (uint32_t seed = 1; seed <= 20; ++seed)
        different += generateWear (seed, kArea, kKeepClear) != a ? 1 : 0;
    CHECK (different == 20);
    CHECK (generateWear (1, kArea, kKeepClear) != generateWear (2, kArea, kKeepClear));
}

TEST_CASE ("wear: tape and stains never cover a keep-clear rectangle")
{
    int tapes = 0, stains = 0;
    for (uint32_t seed = 1; seed <= 200; ++seed)
    {
        for (const auto& item : generateWear (seed * 7919u, kArea, kKeepClear))
        {
            if (item.kind != WearKind::Tape && item.kind != WearKind::Stain)
                continue;
            const auto box = item.kind == WearKind::Tape ? rotatedBounds (item) : item.bounds;
            (item.kind == WearKind::Tape ? tapes : stains)++;
            for (const auto& keep : kKeepClear)
                CHECK_FALSE (box.intersects (keep.toFloat()));
            CHECK (kArea.toFloat().contains (box.reduced (1.0f)));
            if (item.kind == WearKind::Tape)
                CHECK (item.text.isNotEmpty());
        }
    }
    CHECK (tapes > 0);
    CHECK (stains > 0);
}

TEST_CASE ("wear: one to three tape pieces on the compact panel")
{
    int withTape = 0;
    for (uint32_t seed = 1; seed <= 100; ++seed)
    {
        const auto tapes = count (generateWear (seed, kArea, kKeepClear), WearKind::Tape);
        CHECK (tapes <= 3);
        withTape += tapes >= 1 ? 1 : 0;
    }
    CHECK (withTape == 100);
}

TEST_CASE ("wear: the real compact panel always gets at least one tape piece, never the same text twice")
{
    const auto keep = CompactPanel::wearKeepClear();
    int total = 0;
    for (uint32_t seed = 1; seed <= 300; ++seed)
    {
        juce::StringArray texts;
        for (const auto& item : generateWear (seed * 104729u, kArea, keep))
            if (item.kind == WearKind::Tape)
                texts.add (item.text);
        CHECK (texts.size() >= 1);
        CHECK (texts.size() <= 3);
        auto copy = texts;
        copy.removeDuplicates (false);
        CHECK (copy.size() == texts.size());
        total += texts.size();
    }
    CHECK (total > 300);
}

TEST_CASE ("wear: four screws, one per corner")
{
    for (uint32_t seed : { 1u, 99u, 0xdeadbeefu })
    {
        const auto items = generateWear (seed, kArea, kKeepClear);
        REQUIRE (count (items, WearKind::Screw) == 4);

        int corners[4] = { 0, 0, 0, 0 };
        for (const auto& item : items)
        {
            if (item.kind != WearKind::Screw)
                continue;
            const auto c = item.bounds.getCentre();
            corners[(c.x < 260.0f ? 0 : 1) + (c.y < 70.0f ? 0 : 2)]++;
            CHECK (item.variant >= 0);
            CHECK (item.variant <= 2);
        }
        for (const auto n : corners)
            CHECK (n == 1);
    }
}

TEST_CASE ("wear: scratches and screws do not depend on the keep-clear list")
{
    const auto ofKind = [] (const std::vector<WearItem>& items, WearKind kind) {
        std::vector<WearItem> out;
        for (const auto& i : items)
            if (i.kind == kind)
                out.push_back (i);
        return out;
    };

    for (uint32_t seed : { 1u, 7u, 99u, 12345u, 0xdeadbeefu })
    {
        const auto a = generateWear (seed, kArea, kKeepClear);
        const auto b = generateWear (seed, kArea, {});
        const auto c = generateWear (seed, kArea, { { 0, 0, 260, 140 } });
        for (const auto kind : { WearKind::Scratch, WearKind::Screw })
        {
            CHECK (ofKind (a, kind) == ofKind (b, kind));
            CHECK (ofKind (a, kind) == ofKind (c, kind));
        }
    }
}

TEST_CASE ("wear: scales to other panel sizes")
{
    const juce::Rectangle<int> big (0, 0, 900, 520);
    const auto items = generateWear (5, big, {});
    CHECK (count (items, WearKind::Screw) == 4);
    CHECK (count (items, WearKind::Scratch) > count (generateWear (5, kArea, {}), WearKind::Scratch));
    for (const auto& item : items)
        if (item.kind == WearKind::Screw)
            CHECK (big.toFloat().contains (item.bounds));
}

TEST_CASE ("wear: the seed survives the state round trip")
{
    PluginState state;
    CHECK (state.wearSeed == 0);
    for (uint32_t seed : { 1u, 123456789u, 0xdeadbeefu, 0xffffffffu })
    {
        state.wearSeed = seed;
        const auto block = serialize (state);
        const auto loaded = deserialize (block.getData(), block.getSize());
        REQUIRE (loaded.has_value());
        CHECK (loaded->wearSeed == seed);
    }
    CHECK (juce::String::fromUTF8 (static_cast<const char*> (serialize (state).getData()), (int) serialize (state).getSize())
               .contains ("\"wearSeed\""));
}

namespace
{
struct ChangeCounter : juce::AudioProcessorListener
{
    int stateChanges = 0;
    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails& details) override
    {
        stateChanges += details.nonParameterStateChanged ? 1 : 0;
    }
};
} // namespace

TEST_CASE ("wear: a new seed tells the host the state changed, once")
{
    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("swingers-wear-" + juce::Uuid().toString());
    {
        SwingersProcessor processor (root);
        ChangeCounter counter;
        processor.addListener (&counter);
        processor.wearSeed();
        CHECK (counter.stateChanges == 1);
        processor.wearSeed();
        CHECK (counter.stateChanges == 1);
        processor.removeListener (&counter);
    }
    root.deleteRecursively();
}

TEST_CASE ("wear: the processor creates a seed once and keeps it in the project")
{
    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("swingers-wear-" + juce::Uuid().toString());
    {
        SwingersProcessor processor (root);
        const auto seed = processor.wearSeed();
        CHECK (seed != 0);
        CHECK (processor.wearSeed() == seed);

        juce::MemoryBlock saved;
        processor.getStateInformation (saved);
        const auto parsed = deserialize (saved.getData(), saved.getSize());
        REQUIRE (parsed.has_value());
        CHECK (parsed->wearSeed == seed);

        // Another instance loading the project gets the same wear.
        SwingersProcessor other (root);
        other.setStateInformation (saved.getData(), (int) saved.getSize());
        CHECK (other.wearSeed() == seed);

        // A project without a seed (older version) keeps the instance's own.
        PluginState old;
        const auto block = serialize (old);
        other.setStateInformation (block.getData(), (int) block.getSize());
        CHECK (other.wearSeed() == seed);
    }
    root.deleteRecursively();
}

TEST_CASE ("wear: the tape shadow is soft: black at half opacity, offset 0.8 / 1.6, lightly blurred")
{
    juce::Image image (juce::Image::ARGB, 60, 40, true);
    {
        juce::Graphics g (image);
        juce::Path strip;
        strip.addRectangle (10.0f, 10.0f, 30.0f, 12.0f); // centre (25, 16)
        drawTapeShadow (g, strip);
    }

    int peak = 0;
    double sum = 0.0, sx = 0.0, sy = 0.0;
    bool black = true;
    for (int y = 0; y < image.getHeight(); ++y)
        for (int x = 0; x < image.getWidth(); ++x)
        {
            const auto c = image.getPixelAt (x, y);
            const int a = c.getAlpha();
            peak = std::max (peak, a);
            sum += a;
            sx += a * (x + 0.5);
            sy += a * (y + 0.5);
            black = black && (a == 0 || (c.getRed() == 0 && c.getGreen() == 0 && c.getBlue() == 0));
        }

    REQUIRE (sum > 0.0);
    CHECK (black);
    CHECK (peak >= 120); // 0.5 opacity
    CHECK (peak <= 135);
    CHECK (std::abs (sx / sum - 25.0 - 0.8) < 0.25);
    CHECK (std::abs (sy / sum - 16.0 - 1.6) < 0.25);

    // Soft edges: across the middle row the shadow fades over more than the two antialiased pixels of a hard edge.
    int partial = 0;
    for (int x = 0; x < image.getWidth(); ++x)
        if (const int a = image.getPixelAt (x, 17).getAlpha(); a > 0 && a < peak * 9 / 10)
            ++partial;
    CHECK (partial >= 4);
}

TEST_CASE ("wear layers draw something and never take mouse clicks")
{
    juce::ScopedJuceInitialiser_GUI gui;

    WearLayer background (WearLayer::Layer::Background);
    WearLayer tape (WearLayer::Layer::Tape);
    for (auto* layer : { &background, &tape })
    {
        bool clicks = true, childClicks = true;
        layer->getInterceptsMouseClicks (clicks, childClicks);
        CHECK_FALSE (clicks);
        CHECK_FALSE (childClicks);
        layer->setSize (520, 140);
        layer->setKeepClear (kKeepClear);
        layer->setSeed (4242);
    }

    const auto flat = juce::Colour (0xff161617);
    const auto bg = background.createComponentSnapshot (background.getLocalBounds(), false, 1.0f);
    int differing = 0;
    for (int y = 0; y < bg.getHeight(); ++y)
        for (int x = 0; x < bg.getWidth(); ++x)
            differing += bg.getPixelAt (x, y) != flat ? 1 : 0;
    CHECK (differing > 500);
    CHECK (bg.getPixelAt (260, 70).getAlpha() == 255);

    // The tape layer is transparent except where paper and handwriting are.
    const auto tp = tape.createComponentSnapshot (tape.getLocalBounds(), false, 1.0f);
    int painted = 0;
    for (int y = 0; y < tp.getHeight(); ++y)
        for (int x = 0; x < tp.getWidth(); ++x)
            painted += tp.getPixelAt (x, y).getAlpha() > 0 ? 1 : 0;
    CHECK (painted > 300);
    CHECK (painted < 520 * 140 / 4);

    // No wear pixel inside a control.
    for (const auto& keep : { juce::Rectangle<int> (158, 22, 124, 24), juce::Rectangle<int> (150, 78, 302, 26) })
        for (int y = keep.getY(); y < keep.getBottom(); ++y)
            for (int x = keep.getX(); x < keep.getRight(); ++x)
                REQUIRE (tp.getPixelAt (x, y).getAlpha() == 0);

    // Same seed, same picture; another seed, another picture.
    WearLayer again (WearLayer::Layer::Background);
    again.setSize (520, 140);
    again.setKeepClear (kKeepClear);
    again.setSeed (4242);
    const auto bg2 = again.createComponentSnapshot (again.getLocalBounds(), false, 1.0f);
    bool same = true;
    for (int y = 0; y < bg.getHeight() && same; ++y)
        for (int x = 0; x < bg.getWidth() && same; ++x)
            same = bg.getPixelAt (x, y) == bg2.getPixelAt (x, y);
    CHECK (same);
}

TEST_CASE ("editor owns the wear layers and passes keep-clear rectangles on")
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("swingers-wear-" + juce::Uuid().toString());
    {
        SwingersProcessor processor (root);
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        auto* e = dynamic_cast<SwingersEditor*> (editor.get());
        REQUIRE (e != nullptr);
        editor->setVisible (true); // hit testing skips invisible components
        CHECK (processor.wearSeed() != 0);

        auto* background = dynamic_cast<WearLayer*> (editor->findChildWithID ("wearBackground"));
        auto* tape = dynamic_cast<WearLayer*> (editor->findChildWithID ("wearTape"));
        REQUIRE (background != nullptr);
        REQUIRE (tape != nullptr);
        CHECK (background->getSeed() == processor.wearSeed());
        CHECK (tape->getSeed() == processor.wearSeed());

        // Behind and in front of the panel, in that order.
        CHECK (editor->getIndexOfChildComponent (background) < editor->getIndexOfChildComponent (&e->compactPanel()));
        CHECK (editor->getIndexOfChildComponent (&e->compactPanel()) < editor->getIndexOfChildComponent (tape));

        // Clicks go through both layers to the controls.
        auto* hit = editor->getComponentAt (500, 40); // the CH button
        CHECK (hit != nullptr);
        CHECK (hit != background);
        CHECK (hit != tape);

        const std::vector<juce::Rectangle<int>> rects { { 0, 0, 520, 140 } };
        e->setWearKeepClear (rects);
        CHECK (background->getKeepClear() == rects);
        CHECK (tape->getKeepClear() == rects);
    }
    root.deleteRecursively();
}
