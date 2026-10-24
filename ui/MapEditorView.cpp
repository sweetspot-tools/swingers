#include "MapEditorView.h"

#include "RackLookAndFeel.h"

#include <algorithm>

namespace swingers
{
namespace
{
// Geometry from the approved preview (900 x 600).
constexpr int kBoxX = 30, kBoxY = 96, kBoxW = 840, kBoxH = 480;
constexpr int kTableX = 40, kTableY = 130, kTableW = 820, kTableH = 440;
constexpr int kScrollBarThickness = 8;
constexpr int kNameX = 66, kNameW = 300, kGmX = 396, kGmW = 300, kFieldY = 3, kFieldH = 17;
constexpr int kStarX = 760;
constexpr int kNewItemId = 100000; // the unsaved map in the selector
constexpr int kNoneItemId = 1;     // GM voice menu: item id of "none"; voice v has id v + 2

const juce::Colour zebra { 0xff1d1d1f };
const juce::Colour fieldBorder { 0xff2e2e31 };
const juce::Colour dimText { 0xff5a5a5c };
const juce::Colour hintColour { 0xff77777a };

int itemIdOfGm (int gm) { return gm == kNoGm ? kNoneItemId : gm + 2; }
int gmOfItemId (int id) { return id <= kNoneItemId ? kNoGm : id - 2; }

void styleEditor (juce::TextEditor& editor)
{
    editor.setMultiLine (false);
    editor.setReturnKeyStartsNewLine (false);
    editor.setBorder (juce::BorderSize<int>());
    editor.setIndents (6, 0);
    editor.setJustification (juce::Justification::centredLeft);
    editor.setFont (RackLookAndFeel::silkscreenFont (12.0f));
    editor.setColour (juce::TextEditor::backgroundColourId, colours::comboFill);
    editor.setColour (juce::TextEditor::outlineColourId, fieldBorder);
    editor.setColour (juce::TextEditor::focusedOutlineColourId, colours::amber);
    editor.setColour (juce::TextEditor::textColourId, colours::white);
    editor.setColour (juce::TextEditor::highlightColourId, colours::amber);
    editor.setColour (juce::TextEditor::highlightedTextColourId, colours::panel);
    editor.setColour (juce::CaretComponent::caretColourId, colours::amber);
    editor.setSelectAllWhenFocused (false);
}

void setEditorColour (juce::TextEditor& editor, juce::Colour colour)
{
    editor.setColour (juce::TextEditor::textColourId, colour);
    editor.applyColourToAllText (colour);
}
} // namespace

// One row of the table: zebra stripe, note number, name field, GM voice menu and star. Reused by the list.
class MapEditorRow : public juce::Component
{
public:
    explicit MapEditorRow (MapEditorView& o) : owner (o)
    {
        name.setComponentID ("name");
        gm.setComponentID ("gm");
        styleEditor (name);
        name.onTextChange = [this] { owner.setNoteName (note, name.getText()); };
        name.onReturnKey = [this] { name.giveAwayKeyboardFocus(); };
        name.onEscapeKey = [this] {
            name.giveAwayKeyboardFocus();
            refresh (note);
        };

        gm.setJustificationType (juce::Justification::centredLeft);
        gm.onChange = [this] {
            if (! refreshing)
                owner.setNoteGm (note, gmOfItemId (gm.getSelectedId()));
        };
        fillGmMenu (kNoGm);

        addAndMakeVisible (name);
        addAndMakeVisible (gm);
    }

    int noteNumber() const { return note; }

    // Writes what is typed in the field into the map (the field reports changes asynchronously).
    void commit()
    {
        if (! name.isEnabled())
            return;
        const auto* n = owner.editedMap().find (note);
        if (name.getText() != (n != nullptr ? n->name : juce::String()))
            owner.setNoteName (note, name.getText());
    }

    void refresh (int newNote)
    {
        refreshing = true;
        note = newNote;
        const auto* n = owner.editedMap().find (note);
        const bool editable = ! owner.isReadOnly();

        const auto text = n != nullptr ? n->name : juce::String();
        if (name.getText() != text)
            name.setText (text, juce::dontSendNotification);
        name.setEnabled (editable);
        name.setReadOnly (! editable);
        setEditorColour (name, editable ? colours::white : colours::silkscreen.withAlpha (0.7f));

        const auto voice = n != nullptr ? n->gm : kNoGm;
        fillGmMenu (voice);
        gm.setSelectedId (itemIdOfGm (voice), juce::dontSendNotification);
        gm.setEnabled (editable);
        gm.setColour (juce::ComboBox::textColourId, voice == kNoGm ? dimText : colours::white);
        gm.setTooltip ({});
        refreshing = false;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        if (note % 2 == 0)
        {
            g.setColour (zebra);
            g.fillRect (getLocalBounds());
        }

        const auto* n = owner.editedMap().find (note);
        const bool used = n != nullptr && (n->name.isNotEmpty() || n->gm != kNoGm);
        g.setFont (RackLookAndFeel::silkscreenFont (12.0f));
        g.setColour (used ? colours::amber : dimText);
        g.drawText (juce::String (note), juce::Rectangle<int> (10, 0, 44, getHeight()), juce::Justification::centredLeft, false);

        const bool lit = n != nullptr && n->preferred;
        const bool usable = n != nullptr && n->gm != kNoGm && ! owner.isReadOnly();
        juce::Path star;
        star.addStar ({ (float) kStarX, (float) getHeight() * 0.5f }, 5, 3.3f, 7.4f);
        if (lit)
        {
            g.setColour (colours::amber.withMultipliedAlpha (usable ? 1.0f : 0.6f));
            g.fillPath (star);
        }
        else
        {
            g.setColour (usable ? juce::Colour (0xff77777a) : dimText.withMultipliedAlpha (0.7f));
            g.strokePath (star, juce::PathStrokeType (1.1f));
        }
    }

    void resized() override
    {
        name.setBounds (kNameX, kFieldY, kNameW, kFieldH);
        gm.setBounds (kGmX, kFieldY, kGmW, kFieldH);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (std::abs (e.x - kStarX) <= 14)
            owner.togglePreferred (note);
    }

private:
    // "none", then the GM voices as "36  Bass Drum 1"; a voice the GM map does not name gets an item too.
    void fillGmMenu (int voice)
    {
        const bool extra = voice != kNoGm && owner.gmVoiceName (voice).isEmpty();
        const int wanted = extra ? voice : kNoGm;
        if (gm.getNumItems() > 0 && wanted == menuExtra)
            return;

        gm.clear (juce::dontSendNotification);
        gm.addItem ("none", kNoneItemId);
        for (int v = 0; v < 128; ++v)
        {
            const auto& gmName = owner.gmVoiceName (v);
            if (gmName.isNotEmpty())
                gm.addItem (juce::String (v) + "  " + gmName, itemIdOfGm (v));
        }
        if (extra)
            gm.addItem (juce::String (voice), itemIdOfGm (voice));
        menuExtra = wanted;
    }

    MapEditorView& owner;
    int note = 0;
    int menuExtra = kNoGm - 1; // forces the first fill
    bool refreshing = false;
    juce::TextEditor name;
    juce::ComboBox gm;
};

MapEditorView::MapEditorView (SwingersProcessor& p) : processor (p)
{
    setComponentID ("mapEditorView");
    selector.setComponentID ("mapSelector");
    mapNameEditor.setComponentID ("mapName");
    newButton.setComponentID ("new");
    duplicateButton.setComponentID ("duplicate");
    importButton.setComponentID ("import");
    saveButton.setComponentID ("save");
    backButton.setComponentID ("back");
    noteTable.setComponentID ("noteTable");

    if (const auto* gmMap = processor.library().find (kGeneralMidiId))
        for (const auto& n : gmMap->notes)
            if (n.note >= 0 && n.note < 128)
                gmNames[(size_t) n.note] = n.name;

    styleEditor (mapNameEditor);
    mapNameEditor.setFont (RackLookAndFeel::silkscreenFont (12.0f));
    mapNameEditor.setTooltip ("Name of the map as shown in the lists (the file name does not change)");
    mapNameEditor.onTextChange = [this] { setMapName (mapNameEditor.getText()); };
    mapNameEditor.onReturnKey = [this] { mapNameEditor.giveAwayKeyboardFocus(); };

    selector.setTooltip ("The map to edit");
    selector.beforePopup = [this] {
        processor.library().reload(); // maps saved by other instances meanwhile
        refreshMapList();
    };
    selector.onChange = [this] {
        const auto id = selector.getSelectedId();
        if (id == kNewItemId)
            return;
        if (id >= 1 && id <= (int) selectorIds.size())
            loadMapGuarded (selectorIds[(size_t) id - 1]);
    };

    newButton.setTooltip ("Start an empty map");
    duplicateButton.setTooltip ("Make an editable copy of this map");
    importButton.setTooltip ("Read a Cubase drum map (.xml) as a new map");
    saveButton.setTooltip ("Write this map to your maps folder");
    backButton.setTooltip ("Back to the patchbay");

    newButton.onClick = [this] { runGuarded ([this] { startNewMap(); }); };
    duplicateButton.onClick = [this] { runGuarded ([this] { startDuplicate(); }); };
    importButton.onClick = [this] { chooseCubaseFile(); };
    saveButton.onClick = [this] { save(); };
    backButton.onClick = [this] {
        runGuarded ([this] {
            if (onBack)
                onBack();
        });
    };

    askUnsaved = [this] (std::function<void (UnsavedChoice)> reply) { showUnsavedPrompt (std::move (reply)); };

    noteTable.setModel (this);
    noteTable.setRowHeight (kRowHeight);
    noteTable.setWantsKeyboardFocus (false);
    noteTable.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    noteTable.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    noteTable.getViewport()->setScrollBarThickness (kScrollBarThickness);
    noteTable.getViewport()->setScrollBarsShown (true, false);

    addAndMakeVisible (selector);
    addAndMakeVisible (mapNameEditor);
    addAndMakeVisible (newButton);
    addAndMakeVisible (duplicateButton);
    addAndMakeVisible (importButton);
    addAndMakeVisible (saveButton);
    addAndMakeVisible (backButton);
    addAndMakeVisible (noteTable);

    setSize (kWidth, kHeight);
    open();
}

MapEditorView::~MapEditorView()
{
    noteTable.setModel (nullptr);
}

void MapEditorView::open()
{
    const auto& current = processor.current();
    const auto id = current.to.isGeneralMidi() ? current.from.id : current.to.id;
    loadMap (processor.library().find (id) != nullptr ? id : kGeneralMidiId);
}

bool MapEditorView::isReadOnly() const
{
    return ! isNew && processor.library().isBundled (edited.id);
}

juce::String MapEditorView::hintText() const
{
    if (isReadOnly())
        return "Built-in map: duplicate it to edit";
    return juce::String::fromUTF8 ("Notes with no name and no GM voice are not saved  \xc2\xb7  a star marks the preferred note of its GM voice");
}

const juce::String& MapEditorView::gmVoiceName (int voice) const
{
    static const juce::String none;
    return voice >= 0 && voice < 128 ? gmNames[(size_t) voice] : none;
}

void MapEditorView::loadMap (const juce::String& id)
{
    const auto* map = processor.library().find (id);
    if (map == nullptr)
        return;

    edited = *map; // a copy: the library invalidates its pointers on save
    isNew = false;
    dirty = false;
    setStatus ({}, false);
    refreshAll();
    scrollToFirstNote();
}

void MapEditorView::loadMapGuarded (const juce::String& id)
{
    if (! isNew && id == edited.id)
    {
        refreshMapList();
        return;
    }
    runGuarded ([this, id] { loadMap (id); });
}

void MapEditorView::startNewMap()
{
    processor.library().reload(); // other instances may have saved maps: the selector shows them
    edited = processor.library().newMap();
    isNew = true;
    dirty = true;
    setStatus ({}, false);
    refreshAll();
    scrollToFirstNote();
}

void MapEditorView::startDuplicate()
{
    processor.library().reload(); // copy what is on disk now
    auto copy = processor.library().duplicate (edited.id);
    if (copy.name.isEmpty())
        return; // the edited map is not in the library (a new map that was discarded)

    edited = std::move (copy);
    isNew = true;
    dirty = true;
    setStatus ({}, false);
    refreshAll();
    scrollToFirstNote();
}

void MapEditorView::importFile (const juce::File& cubaseXml)
{
    juce::String newId; // importCubase reloads the library first
    if (const auto result = processor.library().importCubase (cubaseXml, newId); result.failed())
    {
        setStatus (result.getErrorMessage(), true);
        repaint();
        return;
    }

    loadMap (newId);
    setStatus ("Imported " + edited.name, false);
    if (onLibraryChanged)
        onLibraryChanged();
    repaint();
}

void MapEditorView::runGuarded (std::function<void()> proceed)
{
    commitPendingEdits();
    if (! dirty)
    {
        proceed();
        return;
    }

    const auto reply = [safe = juce::WeakReference<MapEditorView> (this), proceed = std::move (proceed)] (UnsavedChoice choice) {
        auto* self = safe.get();
        if (self == nullptr)
            return;

        switch (choice)
        {
            case UnsavedChoice::save:
                if (self->save())
                    proceed();
                else
                    self->refreshMapList();
                break;
            case UnsavedChoice::discard:
                self->discardEdits();
                proceed();
                break;
            case UnsavedChoice::cancel:
                self->refreshMapList(); // the selector goes back to the map being edited
                break;
        }
    };

    if (askUnsaved)
        askUnsaved (reply);
    else
        reply (UnsavedChoice::cancel);
}

void MapEditorView::discardEdits()
{
    // Back to what is in the library; a new map that was never saved is dropped for the map in use.
    if (! isNew && processor.library().find (edited.id) != nullptr)
        loadMap (edited.id);
    else
        open();
}

std::optional<SwingersProcessor::MapDraft> MapEditorView::draft()
{
    commitPendingEdits();
    if (! dirty)
        return std::nullopt;
    return SwingersProcessor::MapDraft { edited, isNew };
}

void MapEditorView::restoreDraft (const SwingersProcessor::MapDraft& saved)
{
    edited = saved.map;
    isNew = saved.isNew;
    dirty = true;
    refreshAll();
    scrollToFirstNote();
    setStatus ("Unsaved changes restored", false);
}

void MapEditorView::showUnsavedPrompt (std::function<void (UnsavedChoice)> reply)
{
    const auto name = edited.name.isNotEmpty() ? edited.name : edited.id;
    auto* window = new juce::AlertWindow ("Unsaved changes", "\"" + name + "\" has changes that are not saved.",
                                          juce::MessageBoxIconType::QuestionIcon, this);
    window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Discard", 2);
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    window->enterModalState (true,
                             juce::ModalCallbackFunction::create ([reply = std::move (reply)] (int result) {
                                 reply (result == 1 ? UnsavedChoice::save
                                                    : result == 2 ? UnsavedChoice::discard : UnsavedChoice::cancel);
                             }),
                             true);
}

void MapEditorView::chooseCubaseFile()
{
    const auto start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    chooser = std::make_unique<juce::FileChooser> ("Import a Cubase drum map", start, "*.xml");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe = juce::WeakReference<MapEditorView> (this)] (const juce::FileChooser& fc) {
                              const auto file = fc.getResult();
                              auto* self = safe.get();
                              if (self == nullptr || file == juce::File())
                                  return;
                              self->runGuarded ([self, file] { self->importFile (file); });
                          });
}

MapNote& MapEditorView::ensureNote (int note)
{
    auto it = std::lower_bound (edited.notes.begin(), edited.notes.end(), note,
                                [] (const MapNote& n, int value) { return n.note < value; });
    if (it != edited.notes.end() && it->note == note)
        return *it;

    MapNote created;
    created.note = note;
    return *edited.notes.insert (it, created);
}

void MapEditorView::setMapName (const juce::String& newName)
{
    if (isReadOnly() || newName == edited.name)
        return;
    edited.name = newName;
    if (mapNameEditor.getText() != newName)
        mapNameEditor.setText (newName, juce::dontSendNotification);
    markDirty();
    repaint();
}

void MapEditorView::setNoteName (int note, const juce::String& newName)
{
    if (isReadOnly() || note < 0 || note > 127)
        return;

    const auto* existing = edited.find (note);
    if (existing == nullptr ? newName.isEmpty() : existing->name == newName)
        return;

    ensureNote (note).name = newName;
    markDirty();
    noteTable.updateContent();
}

void MapEditorView::setNoteGm (int note, int gm)
{
    if (isReadOnly() || note < 0 || note > 127 || gm < kNoGm || gm > 127)
        return;

    const auto* existing = edited.find (note);
    if (existing == nullptr ? gm == kNoGm : existing->gm == gm)
        return;

    auto& n = ensureNote (note);
    n.gm = gm;
    n.preferred = false; // the star belongs to a voice
    markDirty();
    noteTable.updateContent();
}

void MapEditorView::togglePreferred (int note)
{
    if (isReadOnly())
        return;

    const auto* existing = edited.find (note);
    if (existing == nullptr || existing->gm == kNoGm)
        return;

    if (existing->preferred)
        ensureNote (note).preferred = false;
    else
        edited.setPreferred (note);
    markDirty();
    noteTable.updateContent();
}

bool MapEditorView::isPreferred (int note) const
{
    const auto* n = edited.find (note);
    return n != nullptr && n->preferred;
}

DrumMap MapEditorView::prunedMap() const
{
    DrumMap out = edited;
    out.notes.clear();
    for (auto n : edited.notes)
    {
        n.name = n.name.trim();
        if (n.name.isEmpty() && n.gm == kNoGm)
            continue; // only used notes are stored
        out.notes.push_back (n);
    }
    return out;
}

bool MapEditorView::save()
{
    if (isReadOnly())
        return false;

    commitPendingEdits();
    auto toSave = prunedMap();
    // A new or duplicated map gets its id now (saveNewMap reloads and picks a free one); a library map is
    // saved over its own file.
    auto& library = processor.library();
    if (! isNew)
        library.reload(); // so the lists show what other instances saved meanwhile
    if (const auto result = isNew ? library.saveNewMap (toSave) : library.saveMap (toSave); result.failed())
    {
        setStatus ("Could not save: " + result.getErrorMessage(), true);
        repaint();
        return false;
    }

    // Back to what is on disk (pruned, sorted), then the processor rebuilds its table when this map is in use.
    if (const auto* saved = processor.library().find (toSave.id))
        edited = *saved;
    isNew = false;
    dirty = false;

    // The processor takes the saved version when this map is its From or To (the project copy is replaced).
    const auto id = toSave.id;
    if (id == processor.current().from.id || id == processor.current().to.id)
        processor.useLibraryVersionOf (id);

    setStatus ("Saved " + (edited.name.isNotEmpty() ? edited.name : edited.id), false);
    refreshAll();
    if (onLibraryChanged)
        onLibraryChanged();
    return true;
}

void MapEditorView::commitPendingEdits()
{
    if (! isReadOnly() && mapNameEditor.getText() != edited.name)
        setMapName (mapNameEditor.getText());

    for (int row = 0; row < 128; ++row)
        if (auto* component = dynamic_cast<MapEditorRow*> (noteTable.getComponentForRowNumber (row)))
            component->commit();
}

juce::Component* MapEditorView::refreshComponentForRow (int row, bool, juce::Component* existing)
{
    if (row < 0 || row > 127)
    {
        delete existing;
        return nullptr;
    }

    auto* component = dynamic_cast<MapEditorRow*> (existing);
    if (component == nullptr)
    {
        delete existing;
        component = new MapEditorRow (*this);
    }
    component->refresh (row);
    return component;
}

void MapEditorView::refreshMapList()
{
    selector.clear (juce::dontSendNotification);
    selectorIds.clear();
    const auto& maps = processor.library().maps();
    for (size_t i = 0; i < maps.size(); ++i)
    {
        selector.addItem (maps[i].name.isNotEmpty() ? maps[i].name : maps[i].id, (int) i + 1);
        selectorIds.push_back (maps[i].id);
    }

    if (isNew)
    {
        selector.addSeparator();
        selector.addItem ((edited.name.isNotEmpty() ? edited.name : edited.id) + " (unsaved)", kNewItemId);
        selector.setSelectedId (kNewItemId, juce::dontSendNotification);
        return;
    }

    for (size_t i = 0; i < selectorIds.size(); ++i)
        if (selectorIds[i] == edited.id)
            selector.setSelectedId ((int) i + 1, juce::dontSendNotification);
}

void MapEditorView::refreshAll()
{
    refreshMapList();
    if (mapNameEditor.getText() != edited.name)
        mapNameEditor.setText (edited.name, juce::dontSendNotification);
    noteTable.updateContent();
    updateControls();
    repaint();
}

void MapEditorView::updateControls()
{
    const bool readOnly = isReadOnly();
    mapNameEditor.setEnabled (! readOnly);
    mapNameEditor.setReadOnly (readOnly);
    setEditorColour (mapNameEditor, readOnly ? colours::silkscreen.withAlpha (0.7f) : colours::white);
    saveButton.setEnabled (! readOnly && dirty);
    duplicateButton.setEnabled (! isNew); // an unsaved map has nothing in the library to copy
}

void MapEditorView::markDirty()
{
    dirty = true;
    if (status.isNotEmpty())
        setStatus ({}, false);
    updateControls();
    if (isNew)
        refreshMapList();
}

void MapEditorView::setStatus (const juce::String& text, bool isError)
{
    status = text;
    statusError = isError && text.isNotEmpty();
    repaint (juce::Rectangle<int> (0, kBoxY + kBoxH, kWidth, kHeight - kBoxY - kBoxH));
}

void MapEditorView::scrollToFirstNote()
{
    const int first = edited.notes.empty() ? 0 : std::max (0, edited.notes.front().note - 2);
    noteTable.getViewport()->setViewPosition (0, first * kRowHeight);
}

std::vector<juce::Rectangle<int>> MapEditorView::wearKeepClear()
{
    return { { 26, 16, 380, 70 },                                      // title, selector and map name
             { 404, 22, 476, 30 },                                     // buttons
             { kBoxX - 3, kBoxY - 3, kBoxW + 6, kBoxH + 6 },           // the NOTES box
             { 30, 576, 840, 18 } };                                   // status line
}

void MapEditorView::paint (juce::Graphics& g)
{
    g.setColour (colours::white);
    g.setFont (RackLookAndFeel::silkscreenFont (22.0f));
    g.drawText ("EDIT MAPS", juce::Rectangle<int> (30, 18, 300, 26), juce::Justification::centredLeft, false);

    RackLookAndFeel::drawBoxWithTitle (g, { kBoxX, kBoxY, kBoxW, kBoxH }, "NOTES", RackLookAndFeel::TitleSide::top);

    g.setFont (RackLookAndFeel::silkscreenFont (10.0f));
    g.setColour (colours::silkscreen);
    for (const auto& [x, label] : { std::pair<int, const char*> { 50, "NOTE" }, { 110, "NAME" },
                                    { 440, "GM VOICE" }, { 780, "PREFERRED" } })
        g.drawText (label, juce::Rectangle<int> (x, 112, 120, 14), juce::Justification::centredLeft, false);

    const auto line = status.isNotEmpty() ? status : hintText();
    g.setColour (statusError ? colours::amber : (status.isNotEmpty() ? colours::silkscreen : hintColour));
    g.drawText (line, juce::Rectangle<int> (30, 578, 840, 14), juce::Justification::centred, true);
}

void MapEditorView::resized()
{
    selector.setBounds (30, 52, 220, 24);
    mapNameEditor.setBounds (258, 52, 142, 24);
    newButton.setBounds (410, 24, 70, 26);
    duplicateButton.setBounds (490, 24, 90, 26);
    importButton.setBounds (590, 24, 150, 26);
    saveButton.setBounds (750, 24, 60, 26);
    backButton.setBounds (820, 24, 52, 26);
    noteTable.setBounds (kTableX, kTableY, kTableW, kTableH);
}
} // namespace swingers
