#pragma once

#include "DrumMap.h"
#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace swingers
{
class MapEditorRow;

// "Edit maps": the table of the 128 notes of one drum map (name, GM voice, preferred star) with New,
// Duplicate, Import from Cubase and Save. It edits a copy of the map; the library is only touched by Save and
// Import. Built-in maps are read-only (Duplicate them). Message thread only.
class MapEditorView : public juce::Component, private juce::ListBoxModel
{
public:
    static constexpr int kWidth = 900;
    static constexpr int kHeight = 600;
    static constexpr int kRowHeight = 23;

    enum class UnsavedChoice { save, discard, cancel };

    explicit MapEditorView (SwingersProcessor&);
    ~MapEditorView() override;

    std::function<void()> onBack;           // BACK, with nothing left unsaved
    std::function<void()> onLibraryChanged; // a map was saved or imported: the lists elsewhere are stale

    // Asks what to do with unsaved changes and calls `reply` with the answer (possibly later). The default
    // shows an async juce::AlertWindow; tests replace it.
    std::function<void (std::function<void (UnsavedChoice)> reply)> askUnsaved;

    // Opens on the processor's To map (From when To is General MIDI). Drops whatever was being edited.
    void open();

    // Unsaved edits to keep when the editor window closes: nullopt when nothing is unsaved.
    std::optional<SwingersProcessor::MapDraft> draft();
    // Reopens such a draft as it was (dirty), with the status "Unsaved changes restored".
    void restoreDraft (const SwingersProcessor::MapDraft&);

    // The map being edited (a copy), with its pending changes.
    const DrumMap& editedMap() const { return edited; }
    bool isDirty() const { return dirty; }       // changes not saved (a new or duplicated map counts)
    bool isReadOnly() const;                     // a built-in map
    juce::String statusText() const { return status; }
    bool statusIsError() const { return statusError; }
    juce::String hintText() const;               // the grey line at the bottom when there is no status

    // Selecting. loadMap drops the pending changes; the *Guarded and button paths ask first (runGuarded).
    void loadMap (const juce::String& id);
    void loadMapGuarded (const juce::String& id);
    void startNewMap();       // an unsaved map of the library's newMap()
    void startDuplicate();    // an unsaved copy of the library version of the edited map
    void importFile (const juce::File& cubaseXml); // saves into the library and selects the new map

    // Runs `proceed` at once when nothing is unsaved; otherwise asks (askUnsaved). Save: proceeds only if
    // the save worked. Discard: proceeds. Cancel: does nothing.
    void runGuarded (std::function<void()> proceed);

    // Edits (ignored on built-in maps). Notes 0..127; gm is kNoGm or 0..127.
    void setMapName (const juce::String&);
    void setNoteName (int note, const juce::String&);
    void setNoteGm (int note, int gm);
    void togglePreferred (int note); // lit star: clears; otherwise takes the star of its GM voice; needs a voice
    bool isPreferred (int note) const;

    // Writes the map to the library, then rebuilds the processor's table when it is the current From or To.
    bool save();

    juce::ListBox& table() { return noteTable; }
    const juce::String& gmVoiceName (int voice) const;

    // Where the wear must stay clear: title and selector, buttons, the box, the status line.
    static std::vector<juce::Rectangle<int>> wearKeepClear();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    int getNumRows() override { return 128; }
    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override {}
    juce::Component* refreshComponentForRow (int row, bool, juce::Component* existing) override;

    void commitPendingEdits();
    void refreshMapList();
    void refreshAll();
    void updateControls();
    void markDirty();
    void setStatus (const juce::String&, bool isError);
    MapNote& ensureNote (int note);
    DrumMap prunedMap() const;
    void showUnsavedPrompt (std::function<void (UnsavedChoice)> reply);
    void chooseCubaseFile();
    void scrollToFirstNote();
    void discardEdits();

    SwingersProcessor& processor;
    DrumMap edited;
    bool dirty = false;
    bool isNew = false; // not in the library yet
    juce::String status;
    bool statusError = false;
    std::array<juce::String, 128> gmNames;

    juce::ComboBox selector;
    std::vector<juce::String> selectorIds; // map id of item (id - 1), as listed when the selector was filled
    juce::TextEditor mapNameEditor;
    juce::TextButton newButton { "New" }, duplicateButton { "Duplicate" },
        importButton { juce::String::fromUTF8 ("Import from Cubase\xe2\x80\xa6") }, saveButton { "Save" },
        backButton { "Back" };
    juce::ListBox noteTable;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_WEAK_REFERENCEABLE (MapEditorView)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MapEditorView)
};
} // namespace swingers
