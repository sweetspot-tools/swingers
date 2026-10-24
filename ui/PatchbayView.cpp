#include "PatchbayView.h"

#include "RackLookAndFeel.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace swingers
{
namespace
{
// Geometry from the approved preview (900 x 600). x values are the same in the view and in the canvas.
constexpr int kX0 = 40, kX1 = 330, kX2 = 620, kColumnWidth = 240;
constexpr int kRowHeight = 33;
constexpr int kBoxTop = 72;                      // preview TOP - 14
constexpr int kBoxInset = 2;                     // the scroller sits this far inside the box, top and bottom
constexpr int kMaxBoxHeight = 498;               // down to y = 570, above the hint line
constexpr int kContentTop = 12;                  // first row, from the scroller's top (preview y = 86)
constexpr int kContentBottom = 14;
constexpr int kScrollBarThickness = 8;
constexpr int kCanvasWidth = PatchbayView::kWidth - kScrollBarThickness;
constexpr float kMaxStretch = 1.6f;              // a short column is spread over the box, up to this x kRowHeight
constexpr float kDragStartDistance = 8.0f;
constexpr float kCullMargin = 20.0f;             // more than half a row or a pill: nothing pops in at the viewport edge
constexpr float kDropReach = 18.0f;              // how far (y) from a target row's centre a cable still connects

const juce::Colour rowFill { 0xff2a2a2c };
const juce::Colour rowBorder { 0xff55555a };
const juce::Colour deadFill { 0xff1c1c1e };
const juce::Colour deadBorder { 0xff2c2c2e };
const juce::Colour dimText { 0xff5a5a5c };
const juce::Colour deadPort { 0xff333333 };
const juce::Colour pillText { 0xff8a8a8d };
const juce::Colour hintText { 0xff77777a };
const juce::Colour litFill { 0xff3b3320 };
const juce::Colour litCable { 0xffffdf8a };

juce::Rectangle<int> leftBox (int height)  { return { kX0 - 10, kBoxTop, kColumnWidth + 20, height }; }
juce::Rectangle<int> rightBox (int height) { return { kX2 - 10, kBoxTop, kColumnWidth + 20, height }; }

float textWidth (const juce::Font& font, const juce::String& text)
{
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (font, text, 0.0f, 0.0f);
    return glyphs.getBoundingBox (0, -1, true).getWidth();
}

juce::String displayName (const DrumMap& map)
{
    return (map.name.isNotEmpty() ? map.name : map.id).toUpperCase();
}

juce::Path cable (juce::Point<float> from, juce::Point<float> to, float reach)
{
    juce::Path path;
    path.startNewSubPath (from);
    path.cubicTo (from.translated (reach, 0.0f), to.translated (-reach, 0.0f), to);
    return path;
}
} // namespace

// The scrolled content: rows, GM pills and cables.
class PatchbayCanvas : public juce::Component
{
public:
    PatchbayCanvas (SwingersProcessor& p, const PatchbayModel& m) : processor (p), model (m)
    {
        srcRowOf.fill (-1);
        tgtRowOf.fill (-1);
        gmY.fill (-1.0f);
        litStart.fill (0);
        lit.fill (false);
        lastHits.fill (0);
        setWantsKeyboardFocus (true);
    }

    std::function<void (int, int)> onConnect;
    std::function<void (int)> onDisconnect;
    std::function<void (int)> onReset;

    // The model changed: recompute the geometry and the voice names.
    void modelChanged()
    {
        gmNames.fill ({});
        if (const auto* gm = processor.library().find (kGeneralMidiId))
            for (const auto& n : gm->notes)
                if (n.note >= 0 && n.note < 128)
                    gmNames[(size_t) n.note] = n.name;

        const auto sources = model.sources.size();
        const auto targets = model.targets.size();
        const auto rows = std::max<size_t> (1, std::max (sources, targets));
        const auto total = (float) rows * (float) kRowHeight;

        const auto place = [&] (size_t n, std::vector<float>& ys, float& rowH) {
            ys.assign (n, 0.0f);
            rowH = (float) kRowHeight - 6.0f;
            if (n == 0)
                return;
            const auto spacing = std::min (total / (float) n, (float) kRowHeight * kMaxStretch);
            const auto offset = (total - (float) n * spacing) * 0.5f;
            for (size_t i = 0; i < n; ++i)
                ys[i] = (float) kContentTop + offset + ((float) i + 0.5f) * spacing;
            rowH = std::min ((float) kRowHeight, spacing) - 6.0f;
        };
        place (sources, srcY, srcRowH);
        place (targets, tgtY, tgtRowH);

        srcRowOf.fill (-1);
        tgtRowOf.fill (-1);
        for (size_t i = 0; i < sources; ++i)
            srcRowOf[(size_t) model.sources[i].note] = (int) i;
        for (size_t i = 0; i < targets; ++i)
            tgtRowOf[(size_t) model.targets[i].note] = (int) i;

        gmY.fill (-1.0f);
        const auto gmTop = (float) kContentTop + (total - (float) model.gmVoices.size() * (float) kRowHeight) * 0.5f;
        for (size_t i = 0; i < model.gmVoices.size(); ++i)
            gmY[(size_t) model.gmVoices[i]] = gmTop + (float) i * (float) kRowHeight + (float) kRowHeight * 0.5f;

        contentHeight = kContentTop + (int) total + kContentBottom;
        dragSource = -1;
        dragging = false;
        syncHits(); // rows that appear (All notes) must not flash for old counts
        setSize (kCanvasWidth, contentHeight);
        repaint();
    }

    // Centre of the GM pill of that voice (canvas coordinates), -1 if it has none.
    int gmPillCentreY (int voice) const
    {
        return voice >= 0 && voice < 128 && gmY[(size_t) voice] >= 0.0f ? (int) std::lround (gmY[(size_t) voice]) : -1;
    }

    int getContentHeight() const { return contentHeight; }

    juce::Rectangle<int> sourceBounds (int note) const
    {
        const auto row = note >= 0 && note < 128 ? srcRowOf[(size_t) note] : -1;
        if (row < 0)
            return {};
        return juce::Rectangle<float> ((float) kX0, srcY[(size_t) row] - srcRowH * 0.5f, (float) kColumnWidth, srcRowH)
            .toNearestInt();
    }

    juce::Rectangle<int> targetBounds (int note) const
    {
        const auto row = note >= 0 && note < 128 ? tgtRowOf[(size_t) note] : -1;
        if (row < 0)
            return {};
        return juce::Rectangle<float> ((float) kX2, tgtY[(size_t) row] - tgtRowH * 0.5f, (float) kColumnWidth, tgtRowH)
            .toNearestInt();
    }

    // Starts counting from the current counters: notes played while the view was hidden do not flash.
    void syncHits()
    {
        for (int note = 0; note < 128; ++note)
        {
            lastHits[(size_t) note] = processor.remapper().hits (note);
            lit[(size_t) note] = false;
        }
    }

    void pollHits (uint32_t nowMs)
    {
        bool changed = false;
        for (const auto& row : model.sources)
        {
            const auto i = (size_t) row.note;
            const auto hits = processor.remapper().hits (row.note);
            if (hits != lastHits[i])
            {
                lastHits[i] = hits;
                litStart[i] = nowMs;
                if (! lit[i])
                {
                    lit[i] = true;
                    changed = true;
                }
            }
            else if (lit[i] && (uint32_t) (nowMs - litStart[i]) >= PatchbayView::kLitMs)
            {
                lit[i] = false;
                changed = true;
            }
        }
        if (changed)
            repaint();
    }

    bool isLit (int note) const { return note >= 0 && note < 128 && lit[(size_t) note]; }

    void paint (juce::Graphics& g) override
    {
        const auto clip = g.getClipBounds().toFloat();
        const auto visible = [&] (float a, float b) {
            return std::max (a, b) + kCullMargin >= clip.getY() && std::min (a, b) - kCullMargin <= clip.getBottom();
        };

        if (model.showGmColumn)
        {
            g.setColour (dimText);
            g.setFont (RackLookAndFeel::silkscreenFont (10.0f));
            g.drawText ("GENERAL MIDI", juce::Rectangle<int> (kX1, kContentTop - 14, kColumnWidth, 13),
                        juce::Justification::centred, false);
        }

        // Standard paths, faint, through the GM column; lit ones in amber after the quiet ones.
        for (int pass = 0; pass < 2; ++pass)
        {
            for (const auto& c : model.cables)
            {
                if (c.direct || isLit (c.source) != (pass == 1))
                    continue;
                const auto sy = srcY[(size_t) srcRowOf[(size_t) c.source]];
                const auto ti = tgtRowOf[(size_t) c.target];
                if (ti < 0)
                    continue;
                const auto ty = tgtY[(size_t) ti];
                const bool throughGm = model.showGmColumn && c.gm >= 0 && gmY[(size_t) c.gm] >= 0.0f;
                // The cable also goes through its GM pill, which can be far from both ends.
                const auto gy = throughGm ? gmY[(size_t) c.gm] : sy;
                if (! visible (std::min ({ sy, ty, gy }), std::max ({ sy, ty, gy })))
                    continue;

                g.setColour (pass == 1 ? colours::amber : colours::silkscreen.withAlpha (0.3f));
                const juce::PathStrokeType stroke (pass == 1 ? 2.5f : 2.0f);
                const juce::Point<float> from ((float) (kX0 + kColumnWidth), sy), to ((float) kX2, ty);
                if (throughGm)
                {
                    g.strokePath (cable (from, { (float) (kX1 + 50), gy }, 50.0f), stroke);
                    g.strokePath (cable ({ (float) (kX1 + kColumnWidth - 50), gy }, to, 30.0f), stroke);
                }
                else
                {
                    g.strokePath (cable (from, to, 120.0f), stroke);
                }
            }
        }

        // GM voices: dark pills with the voice name, above the standard paths.
        if (model.showGmColumn)
        {
            g.setFont (RackLookAndFeel::silkscreenFont (11.0f));
            for (const auto voice : model.gmVoices)
            {
                const auto y = gmY[(size_t) voice];
                if (! visible (y, y))
                    continue;
                const auto pill = juce::Rectangle<float> ((float) kX1 + 40.0f, y - 10.0f, (float) kColumnWidth - 80.0f, 20.0f);
                g.setColour (colours::panel);
                g.fillRoundedRectangle (pill, 10.0f);
                g.setColour (deadBorder);
                g.drawRoundedRectangle (pill.reduced (0.5f), 10.0f, 1.0f);
                g.setColour (pillText);
                g.drawText (juce::String (voice) + "  " + gmNames[(size_t) voice], pill.reduced (8.0f, 0.0f).toNearestInt(),
                            juce::Justification::centred, true);
            }
        }

        // Direct cables: amber, edge to edge, above the GM labels.
        for (const auto& c : model.cables)
        {
            if (! c.direct)
                continue;
            const auto ti = tgtRowOf[(size_t) c.target];
            if (ti < 0)
                continue;
            const auto sy = srcY[(size_t) srcRowOf[(size_t) c.source]];
            const auto ty = tgtY[(size_t) ti];
            if (! visible (sy, ty))
                continue;
            g.setColour (isLit (c.source) ? litCable : colours::amber);
            g.strokePath (cable ({ (float) (kX0 + kColumnWidth), sy }, { (float) kX2, ty }, 180.0f),
                          juce::PathStrokeType (isLit (c.source) ? 3.5f : 3.0f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
        }

        drawSources (g, visible);
        drawTargets (g, visible);

        if (dragging && dragSource >= 0)
        {
            const auto sy = srcY[(size_t) dragSource];
            juce::Path path = cable ({ (float) (kX0 + kColumnWidth), sy }, dragPoint, 150.0f);
            juce::Path dashed;
            const float dashes[] = { 6.0f, 5.0f };
            juce::PathStrokeType (3.0f).createDashedStroke (dashed, path, dashes, 2);
            g.setColour (colours::amber);
            g.fillPath (dashed);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragSource = -1;
        dragging = false;
        const auto row = sourceRowAt (e.position);
        if (row < 0)
            return;

        const auto& source = model.sources[(size_t) row];
        if (e.mods.isPopupMenu())
        {
            showMenu (source);
            return;
        }
        if (! e.mods.isLeftButtonDown() || source.state == SourceState::DroppedByUser)
            return;
        dragSource = row;
        dragPoint = e.position;
        grabKeyboardFocus(); // Escape cancels the cable
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key != juce::KeyPress::escapeKey || dragSource < 0)
            return false;
        cancelDrag();
        return true;
    }

    void cancelDrag()
    {
        dragSource = -1;
        dragging = false;
        hoverTarget = -1;
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragSource < 0)
            return;
        if (! dragging && e.getDistanceFromDragStart() < kDragStartDistance)
            return;
        dragging = true;
        dragPoint = e.position;
        hoverTarget = targetRowAt (e.position);
        repaint();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (dragSource < 0)
            return;
        const auto note = model.sources[(size_t) dragSource].note;
        const auto wasDragging = dragging;
        const auto target = wasDragging ? targetRowAt (e.position) : -1;
        const auto targetNote = target >= 0 ? model.targets[(size_t) target].note : -1;
        // Empty space: inside the canvas, right of the left box (not on the source rows, which would be a click
        // with a slip of the mouse), and not on a target.
        const bool inEmptySpace = getLocalBounds().toFloat().contains (e.position)
                                  && e.position.x > (float) (kX0 + kColumnWidth + 10);

        cancelDrag();

        if (! wasDragging)
            return; // a plain click does nothing
        if (targetNote >= 0)
        {
            if (onConnect)
                onConnect (note, targetNote);
        }
        else if (inEmptySpace && onDisconnect)
        {
            onDisconnect (note);
        }
    }

private:
    int sourceRowAt (juce::Point<float> p) const
    {
        if (p.x < (float) kX0 || p.x > (float) (kX0 + kColumnWidth + 8))
            return -1;
        for (size_t i = 0; i < srcY.size(); ++i)
            if (std::abs (p.y - srcY[i]) <= srcRowH * 0.5f)
                return (int) i;
        return -1;
    }

    // The nearest target row to the right edge, within reach: the cable connects to it.
    int targetRowAt (juce::Point<float> p) const
    {
        if (p.x < (float) (kX2 - 10) || p.x > (float) (kX2 + kColumnWidth + 10))
            return -1; // only inside the target box
        int best = -1;
        auto bestDistance = kDropReach;
        for (size_t i = 0; i < tgtY.size(); ++i)
        {
            const auto d = std::abs (tgtY[i] - p.y);
            if (d < bestDistance)
            {
                bestDistance = d;
                best = (int) i;
            }
        }
        return best;
    }

    void showMenu (const PatchRow& source)
    {
        const auto& overrides = processor.current().overrides;
        const bool dropped = source.state == SourceState::DroppedByUser;
        const bool hasOverride = dropped || overrides.direct.count (source.note) > 0 || overrides.dropped.count (source.note) > 0;

        juce::PopupMenu menu;
        if (! dropped)
            menu.addItem (1, "Disconnect");
        if (hasOverride)
            menu.addItem (2, "Reset");
        if (menu.getNumItems() == 0)
            return;

        const auto note = source.note;
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                            [safe = juce::Component::SafePointer<PatchbayCanvas> (this), note] (int result) {
                                if (safe == nullptr)
                                    return;
                                if (result == 1 && safe->onDisconnect)
                                    safe->onDisconnect (note);
                                else if (result == 2 && safe->onReset)
                                    safe->onReset (note);
                            });
    }

    template <typename Visible>
    void drawSources (juce::Graphics& g, const Visible& visible)
    {
        const auto numberFont = RackLookAndFeel::silkscreenFont (12.0f);
        const auto tagFont = RackLookAndFeel::silkscreenFont (9.0f);

        for (size_t i = 0; i < model.sources.size(); ++i)
        {
            const auto& row = model.sources[i];
            const auto cy = srcY[i];
            if (! visible (cy, cy))
                continue;

            const bool dropped = row.state == SourceState::DroppedByUser;
            const bool dead = dropped || row.state == SourceState::NoMatch;
            const bool active = isLit (row.note) || (dragging && (int) i == dragSource);
            const auto rect = juce::Rectangle<float> ((float) kX0, cy - srcRowH * 0.5f, (float) kColumnWidth, srcRowH);

            g.setColour (active ? litFill : (dead ? deadFill : rowFill));
            g.fillRoundedRectangle (rect, 4.0f);
            g.setColour (active ? colours::amber : (dead ? deadBorder : rowBorder));
            g.drawRoundedRectangle (rect.reduced (0.5f), 4.0f, 1.0f);

            g.setFont (numberFont);
            g.setColour (dead ? dimText : colours::amber);
            g.drawText (juce::String (row.note), juce::Rectangle<float> (rect.getX() + 12.0f, cy - 8.0f, 32.0f, 16.0f).toNearestInt(),
                        juce::Justification::centredLeft, false);

            const auto nameRight = dead ? 84.0f : 12.0f;
            g.setColour (dead ? dimText : colours::white);
            g.drawText (row.name, juce::Rectangle<float> (rect.getX() + 44.0f, cy - 8.0f, rect.getWidth() - 44.0f - nameRight, 16.0f).toNearestInt(),
                        juce::Justification::centredLeft, true);

            g.setFont (tagFont);
            g.setColour (dimText);
            if (dropped)
            {
                g.drawLine ((float) kX0 + 40.0f, cy, (float) (kX0 + kColumnWidth) - 14.0f, cy, 1.5f);
                g.drawText ("DISCONNECTED", juce::Rectangle<float> (rect.getRight() - 90.0f, cy - 7.0f, 80.0f, 14.0f).toNearestInt(),
                            juce::Justification::centredRight, false);
            }
            else if (row.state == SourceState::NoMatch)
            {
                g.drawText ("NO MATCH", juce::Rectangle<float> (rect.getRight() - 90.0f, cy - 7.0f, 80.0f, 14.0f).toNearestInt(),
                            juce::Justification::centredRight, false);
            }

            g.setColour (dead ? deadPort : colours::white);
            g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ rect.getRight(), cy }));
        }
    }

    template <typename Visible>
    void drawTargets (juce::Graphics& g, const Visible& visible)
    {
        const auto numberFont = RackLookAndFeel::silkscreenFont (12.0f);

        for (size_t i = 0; i < model.targets.size(); ++i)
        {
            const auto& t = model.targets[i];
            const auto cy = tgtY[i];
            if (! visible (cy, cy))
                continue;

            const bool undefined = t.name.isEmpty() && t.gm == kNoGm;
            const bool hovered = dragging && (int) i == hoverTarget;
            const auto rect = juce::Rectangle<float> ((float) kX2, cy - tgtRowH * 0.5f, (float) kColumnWidth, tgtRowH);

            g.setColour (undefined ? deadFill : rowFill);
            g.fillRoundedRectangle (rect, 4.0f);
            g.setColour (hovered ? colours::amber : (undefined ? deadBorder : rowBorder));
            g.drawRoundedRectangle (rect.reduced (0.5f), 4.0f, hovered ? 1.6f : 1.0f);

            g.setColour (undefined ? deadPort : colours::white);
            g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ rect.getX(), cy }));

            g.setFont (numberFont);
            g.setColour (undefined ? dimText : colours::amber);
            g.drawText (juce::String (t.note), juce::Rectangle<float> (rect.getX() + 14.0f, cy - 8.0f, 32.0f, 16.0f).toNearestInt(),
                        juce::Justification::centredLeft, false);
            g.setColour (colours::white);
            g.drawText (t.name, juce::Rectangle<float> (rect.getX() + 46.0f, cy - 8.0f, rect.getWidth() - 46.0f - (t.preferred ? 26.0f : 12.0f), 16.0f).toNearestInt(),
                        juce::Justification::centredLeft, true);

            if (t.preferred)
            {
                juce::Path star;
                star.addStar ({ rect.getRight() - 16.0f, cy }, 5, 2.7f, 6.2f);
                g.setColour (colours::amber);
                g.fillPath (star);
            }
        }
    }

    SwingersProcessor& processor;
    const PatchbayModel& model;

    std::array<juce::String, 128> gmNames;
    std::vector<float> srcY, tgtY; // row centres, canvas coordinates
    float srcRowH = 0.0f, tgtRowH = 0.0f;
    std::array<float, 128> gmY;
    std::array<int, 128> srcRowOf, tgtRowOf;
    int contentHeight = 0;

    std::array<uint32_t, 128> lastHits, litStart;
    std::array<bool, 128> lit;

    int dragSource = -1; // row in model.sources
    bool dragging = false;
    int hoverTarget = -1;
    juce::Point<float> dragPoint;
};

PatchbayView::PatchbayView (SwingersProcessor& p)
    : processor (p), canvasComponent (std::make_unique<PatchbayCanvas> (p, patchModel))
{
    allNotesButton.setComponentID ("allNotes");
    editMapsButton.setComponentID ("editMaps");
    doneButton.setComponentID ("done");
    scroller.setComponentID ("patchbayScroller");
    canvasComponent->setComponentID ("patchbayCanvas");

    allNotesButton.setClickingTogglesState (true);
    allNotesButton.setTooltip ("Show all 128 notes instead of the ones the maps define");
    allNotesButton.onClick = [this] { setAllNotes (allNotesButton.getToggleState()); };
    editMapsButton.setTooltip ("Edit the notes of the maps");
    editMapsButton.onClick = [this] {
        if (editMapsHandler)
            editMapsHandler();
    };
    doneButton.onClick = [this] {
        if (onDone)
            onDone();
    };

    canvasComponent->onConnect = [this] (int source, int target) { connectNotes (source, target); };
    canvasComponent->onDisconnect = [this] (int source) { disconnectNote (source); };
    canvasComponent->onReset = [this] (int source) { resetNote (source); };

    scroller.setViewedComponent (canvasComponent.get(), false);
    scroller.setScrollBarsShown (true, false);
    scroller.setScrollBarThickness (kScrollBarThickness);
    scroller.setSingleStepSizes (0, kRowHeight);

    addAndMakeVisible (scroller);
    addAndMakeVisible (allNotesButton);
    addAndMakeVisible (editMapsButton);
    addAndMakeVisible (doneButton);

    setSize (kWidth, kHeight);
    refresh();
}

PatchbayView::~PatchbayView()
{
    stopTimer();
    scroller.setViewedComponent (nullptr, false);
}

void PatchbayView::refresh()
{
    const auto& current = processor.current();
    patchModel = PatchbayModel::build (current.from, current.to,
                                       ConversionTable::build (current.from, current.to, &current.overrides), allNotes);
    canvasComponent->modelChanged();
    layoutContent();
    updateEditMapsEnabled();
    repaint();
}

void PatchbayView::setEditMapsHandler (std::function<void()> handler)
{
    editMapsHandler = std::move (handler);
    updateEditMapsEnabled();
}

void PatchbayView::setAllNotes (bool shouldShowAll)
{
    allNotesButton.setToggleState (shouldShowAll, juce::dontSendNotification);
    if (allNotes == shouldShowAll)
        return;
    allNotes = shouldShowAll;
    scroller.setViewPosition (0, 0);
    refresh();

    // With all 128 rows, start at the first note the maps know, not at the empty low notes.
    if (allNotes)
    {
        for (const auto& row : patchModel.sources)
        {
            if (row.name.isNotEmpty() || row.state != SourceState::NoMatch)
            {
                const auto bounds = canvasComponent->sourceBounds (row.note);
                scroller.setViewPosition (0, std::max (0, bounds.getY() - kRowHeight));
                break;
            }
        }
    }
}

bool PatchbayView::isAllNotes() const
{
    return allNotes;
}

void PatchbayView::edit (const std::function<void (PairOverrides&)>& change)
{
    const auto& current = processor.current();
    auto overrides = current.overrides;
    overrides.fromId = current.from.id;
    overrides.toId = current.to.id;
    change (overrides);
    processor.applyOverrides (overrides);
}

void PatchbayView::connectNotes (int sourceNote, int targetNote)
{
    edit ([=] (PairOverrides& o) { o.connect (sourceNote, targetNote); });
}

void PatchbayView::disconnectNote (int sourceNote)
{
    edit ([=] (PairOverrides& o) { o.drop (sourceNote); });
}

void PatchbayView::resetNote (int sourceNote)
{
    edit ([=] (PairOverrides& o) { o.reset (sourceNote); });
}

void PatchbayView::pollHits (uint32_t nowMs)
{
    canvasComponent->pollHits (nowMs);
}

bool PatchbayView::isLit (int sourceNote) const
{
    return canvasComponent->isLit (sourceNote);
}

juce::Component& PatchbayView::canvas()
{
    return *canvasComponent;
}

int PatchbayView::gmPillCentreY (int voice) const
{
    return canvasComponent->gmPillCentreY (voice);
}

juce::Rectangle<int> PatchbayView::sourceRowBounds (int note) const
{
    return canvasComponent->sourceBounds (note);
}

juce::Rectangle<int> PatchbayView::targetRowBounds (int note) const
{
    return canvasComponent->targetBounds (note);
}

juce::Rectangle<int> PatchbayView::boxesArea() const
{
    return leftBox (boxHeight).getUnion (rightBox (boxHeight));
}

std::vector<juce::Rectangle<int>> PatchbayView::wearKeepClear() const
{
    return { boxesArea().expanded (3),
             { 26, 16, 300, 50 },                                   // title and from/to line
             { 498, 22, 376, 30 },                                  // buttons
             { 140, 576, 620, 18 },                                 // hint
             { kWidth - kScrollBarThickness - 2, kBoxTop, kScrollBarThickness + 2, boxHeight } };
}

void PatchbayView::layoutContent()
{
    const auto newBoxHeight = std::min (canvasComponent->getContentHeight() + 2 * kBoxInset, kMaxBoxHeight);
    const auto scrolled = scroller.getViewPositionY();
    scroller.setBounds (0, kBoxTop + kBoxInset, kWidth, newBoxHeight - 2 * kBoxInset);
    scroller.setViewPosition (0, scrolled);

    if (newBoxHeight != boxHeight)
    {
        boxHeight = newBoxHeight;
        repaint();
        if (onLayoutChanged)
            onLayoutChanged();
    }
}

void PatchbayView::updateEditMapsEnabled()
{
    editMapsButton.setEnabled (static_cast<bool> (editMapsHandler));
}

void PatchbayView::resized()
{
    allNotesButton.setBounds (500, 24, 108, 26);
    editMapsButton.setBounds (620, 24, 110, 26);
    doneButton.setBounds (742, 24, 130, 26);
    layoutContent();
}

void PatchbayView::visibilityChanged()
{
    if (isVisible())
    {
        updateEditMapsEnabled();
        canvasComponent->syncHits();
        startTimerHz (30);
    }
    else
    {
        stopTimer();
    }
}

void PatchbayView::timerCallback()
{
    pollHits (juce::Time::getMillisecondCounter());
}

void PatchbayView::paint (juce::Graphics& g)
{
    const auto& current = processor.current();
    const auto fromName = displayName (current.from);
    const auto toName = displayName (current.to);

    g.setColour (colours::white);
    g.setFont (RackLookAndFeel::silkscreenFont (22.0f));
    g.drawText ("EDIT MAPPING", juce::Rectangle<int> (30, 18, 300, 26), juce::Justification::centredLeft, false);

    // FROM -> TO under the title; the arrow is a path, not a glyph the font might lack.
    const auto font = RackLookAndFeel::silkscreenFont (11.0f);
    const auto fromWidth = (int) std::ceil (textWidth (font, fromName));
    const auto toWidth = (int) std::ceil (textWidth (font, toName));
    g.setColour (colours::silkscreen);
    g.setFont (font);
    g.drawText (fromName, juce::Rectangle<int> (30, 47, fromWidth + 2, 14), juce::Justification::centredLeft, false);
    const auto arrowX = 30.0f + (float) fromWidth + 10.0f;
    g.drawArrow ({ arrowX, 54.0f, arrowX + 16.0f, 54.0f }, 1.6f, 8.0f, 7.0f);
    g.drawText (toName, juce::Rectangle<int> ((int) arrowX + 26, 47, toWidth + 2, 14), juce::Justification::centredLeft, false);

    RackLookAndFeel::drawBoxWithTitle (g, leftBox (boxHeight), fromName, RackLookAndFeel::TitleSide::top);
    RackLookAndFeel::drawBoxWithTitle (g, rightBox (boxHeight), toName, RackLookAndFeel::TitleSide::top);

    g.setColour (hintText);
    g.setFont (RackLookAndFeel::silkscreenFont (10.0f));
    g.drawText (juce::String::fromUTF8 ("Drag a note on the left onto a note on the right to connect it directly  \xc2\xb7  right-click a note on the left to disconnect or reset it"),
                juce::Rectangle<int> (30, 578, 840, 14), juce::Justification::centred, false);
}
} // namespace swingers
