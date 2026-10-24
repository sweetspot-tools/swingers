#include <catch2/catch_test_macros.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include "CompactPanel.h"
#include "MapLibrary.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "PluginState.h"

using namespace swingers;

namespace
{
struct TempRoot
{
    juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("swingers-editor-" + juce::Uuid().toString());
    ~TempRoot() { dir.deleteRecursively(); }
};

// One valid user map (Kick on 35) and one broken file.
void prepareRoot (const juce::File& root)
{
    DrumMap map;
    map.id = "test-kit";
    map.name = "Test Kit";
    map.library = "Test";
    map.notes = { { 35, "Kick", 36, false } };
    MapLibrary writer (root);
    REQUIRE (writer.saveMap (map).wasOk());
    REQUIRE (root.getChildFile ("maps/broken.swmap.json").replaceWithText ("{ not json"));
}

template <typename T>
T& child (juce::Component& parent, const char* id)
{
    auto* found = dynamic_cast<T*> (parent.findChildWithID (id));
    REQUIRE (found != nullptr);
    return *found;
}

juce::StringArray itemTexts (juce::ComboBox& box)
{
    juce::StringArray texts;
    for (int i = 0; i < box.getNumItems(); ++i)
        texts.add (box.getItemText (i));
    return texts;
}

// triggerClick posts a message: let the queue run for a moment.
void pumpMessages()
{
    const auto end = juce::Time::getMillisecondCounter() + 100;
    while (juce::Time::getMillisecondCounter() < end)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
}

int idOfItem (juce::ComboBox& box, const juce::String& text)
{
    for (int i = 0; i < box.getNumItems(); ++i)
        if (box.getItemText (i) == text)
            return box.getItemId (i);
    FAIL ("item not found: " << text);
    return 0;
}
} // namespace

TEST_CASE ("editor shows the compact panel")
{
    juce::ScopedJuceInitialiser_GUI gui;
    TempRoot tmp;
    prepareRoot (tmp.dir);
    SwingersProcessor processor (tmp.dir);

    REQUIRE (processor.hasEditor());
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    REQUIRE (editor != nullptr);
    CHECK (editor->getWidth() == 520);
    CHECK (editor->getHeight() == 140); // the warnings row fits in 140

    auto& panel = child<CompactPanel> (*editor, "compactPanel");
    auto& from = child<juce::ComboBox> (panel, "fromBox");
    auto& to = child<juce::ComboBox> (panel, "toBox");
    auto& channel = child<ChannelCycle> (panel, "channelBox");

    for (auto* box : { &from, &to })
    {
        const auto texts = itemTexts (*box);
        REQUIRE (texts.size() == 3);
        CHECK (texts[0] == "General MIDI");
        CHECK (texts[1] == "Test Kit");
        CHECK (texts[2] == "broken.swmap.json");
        CHECK (box->isItemEnabled (box->getItemId (0)));
        CHECK (box->isItemEnabled (box->getItemId (1)));
        CHECK_FALSE (box->isItemEnabled (box->getItemId (2)));
        CHECK (box->getTooltip().contains ("broken.swmap.json"));
        CHECK (box->getTooltip().contains ("Invalid JSON"));
    }
    CHECK (from.getText() == "General MIDI");
    CHECK (to.getText() == "General MIDI");

    CHECK (channel.getValueText() == "ALL");
    CHECK_FALSE (channel.isLedLit());
}

TEST_CASE ("choosing items drives the processor")
{
    juce::ScopedJuceInitialiser_GUI gui;
    TempRoot tmp;
    prepareRoot (tmp.dir);
    SwingersProcessor processor (tmp.dir);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    auto& panel = child<CompactPanel> (*editor, "compactPanel");
    auto& from = child<juce::ComboBox> (panel, "fromBox");
    auto& to = child<juce::ComboBox> (panel, "toBox");
    auto& channel = child<ChannelCycle> (panel, "channelBox");

    to.setSelectedId (idOfItem (to, "Test Kit"), juce::sendNotificationSync);
    CHECK (processor.current().to.id == "test-kit");
    CHECK (processor.current().from.id == "general-midi");
    CHECK (to.getText() == "Test Kit");

    from.setSelectedId (idOfItem (from, "Test Kit"), juce::sendNotificationSync);
    CHECK (processor.current().from.id == "test-kit");
    CHECK (processor.current().to.id == "test-kit"); // the other side is kept

    // A click cycles ALL -> 1 -> ... -> 16 -> ALL, the LED is lit whenever the value is not ALL.
    CHECK (channel.getValueText() == "ALL");
    for (int expected = 1; expected <= 16; ++expected)
    {
        channel.cycle();
        CHECK (channel.getValueText() == juce::String (expected));
        CHECK (processor.channel() == expected);
        CHECK (channel.isLedLit());
    }
    channel.cycle();
    CHECK (channel.getValueText() == "ALL");
    CHECK (processor.channel() == 0);
    CHECK_FALSE (channel.isLedLit());

    // A real mouse click does the same.
    const auto click = juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), { 10.0f, 10.0f },
                                         juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &channel, &channel,
                                         juce::Time::getCurrentTime(), { 10.0f, 10.0f }, juce::Time::getCurrentTime(),
                                         1, false);
    channel.mouseUp (click);
    CHECK (processor.channel() == 1);
}

TEST_CASE ("panel follows state changes made elsewhere")
{
    juce::ScopedJuceInitialiser_GUI gui;
    TempRoot tmp;
    prepareRoot (tmp.dir);
    SwingersProcessor processor (tmp.dir);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    auto& panel = child<CompactPanel> (*editor, "compactPanel");
    auto& to = child<juce::ComboBox> (panel, "toBox");
    auto& channel = child<ChannelCycle> (panel, "channelBox");
    auto& warnings = child<juce::Label> (panel, "warnings");

    CHECK (warnings.getText().isEmpty());

    processor.selectMaps ("general-midi", "test-kit");
    processor.setChannel (4);
    CHECK (to.getText() == "Test Kit");
    CHECK (channel.getValueText() == "4");

    // A project that names a map this computer lacks: General MIDI plus a warning in the row.
    PluginState state;
    state.fromId = "general-midi";
    state.toId = "gone-kit";
    DrumMap gone;
    gone.id = "gone-kit";
    gone.name = "Gone Kit";
    gone.notes = { { 35, "Kick", 36, false } };
    state.toCopy = gone;
    state.channel = 2;
    const auto block = serialize (state);
    processor.setStateInformation (block.getData(), (int) block.getSize());

    CHECK (processor.current().warnings.size() > 0);
    CHECK (warnings.getText().isNotEmpty());
    CHECK (warnings.getText() == processor.current().warnings.joinIntoString ("  |  "));
    CHECK (to.getText() == "Gone Kit"); // the project's copy, named although it is not in the list
    CHECK (channel.getValueText() == "2");
}

TEST_CASE ("Edit mapping asks for the patchbay")
{
    juce::ScopedJuceInitialiser_GUI gui;
    TempRoot tmp;
    prepareRoot (tmp.dir);
    SwingersProcessor processor (tmp.dir);

    SECTION ("panel callback")
    {
        CompactPanel panel (processor);
        int calls = 0;
        panel.onEditMapping = [&] { ++calls; };
        child<juce::TextButton> (panel, "editMapping").triggerClick();
        pumpMessages(); // triggerClick is asynchronous
        CHECK (calls == 1);
    }

    SECTION ("editor placeholder")
    {
        SwingersEditor editor (processor);
        CHECK_FALSE (editor.isPatchbayShown());
        child<juce::TextButton> (editor.compactPanel(), "editMapping").triggerClick();
        pumpMessages();
        CHECK (editor.isPatchbayShown());
        editor.showPatchbay (false);
        CHECK_FALSE (editor.isPatchbayShown());
    }
}

TEST_CASE ("LED lights for 80 ms after a routed note-on")
{
    juce::ScopedJuceInitialiser_GUI gui;
    TempRoot tmp;
    prepareRoot (tmp.dir);
    SwingersProcessor processor (tmp.dir);
    processor.selectMaps ("general-midi", "test-kit"); // GM kick 36 -> note 35

    CompactPanel panel (processor);
    auto& led = child<ActivityLed> (panel, "led");

    panel.pollActivity (1000);
    CHECK_FALSE (led.isLit());
    CHECK_FALSE (panel.isLedLit());

    juce::AudioBuffer<float> audio (0, 16);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 100), 0);
    processor.processBlock (audio, midi);

    panel.pollActivity (2000);
    CHECK (led.isLit());
    panel.pollActivity (2079);
    CHECK (led.isLit());
    panel.pollActivity (2080);
    CHECK_FALSE (led.isLit());

    // A second hit relights it.
    juce::MidiBuffer again;
    again.addEvent (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 100), 0);
    processor.processBlock (audio, again);
    panel.pollActivity (3000);
    CHECK (led.isLit());
}

TEST_CASE ("warnings label can show its tooltip and still blocks nothing")
{
    juce::ScopedJuceInitialiser_GUI gui;
    TempRoot tmp;
    prepareRoot (tmp.dir);
    SwingersProcessor processor (tmp.dir);

    PluginState state;
    state.toId = "gone-kit";
    DrumMap gone;
    gone.id = "gone-kit";
    gone.name = "Gone Kit";
    gone.notes = { { 35, "Kick", 36, false } };
    state.toCopy = gone;
    const auto block = serialize (state);
    processor.setStateInformation (block.getData(), (int) block.getSize());

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setVisible (true); // hit testing skips invisible components
    auto& panel = child<CompactPanel> (*editor, "compactPanel");
    auto& warnings = child<juce::Label> (panel, "warnings");
    REQUIRE (warnings.getText().isNotEmpty());
    CHECK (warnings.getTooltip().isNotEmpty());

    // The tooltip window asks the component under the mouse: it must be the label, through both wear layers.
    const auto centre = editor->getLocalPoint (&warnings, warnings.getLocalBounds().getCentre());
    CHECK (editor->getComponentAt (centre) == &warnings);

    // The label sits on the warnings row only: the controls above it are still reached.
    CHECK (editor->getComponentAt (editor->getLocalPoint (&child<ChannelCycle> (panel, "channelBox"), juce::Point<int> (24, 29)))
           == &child<ChannelCycle> (panel, "channelBox"));
}

TEST_CASE ("processor stays usable after the editor is destroyed")
{
    juce::ScopedJuceInitialiser_GUI gui;
    TempRoot tmp;
    prepareRoot (tmp.dir);
    SwingersProcessor processor (tmp.dir);

    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        REQUIRE (editor != nullptr);
    }

    // The editor cleared the refresh callback: nothing may call into the destroyed panel.
    processor.selectMaps ("general-midi", "test-kit");
    processor.setChannel (3);
    CHECK (processor.current().to.id == "test-kit");
    CHECK (processor.channel() == 3);
    CHECK_FALSE (static_cast<bool> (processor.onStateChanged));
}

TEST_CASE ("a map saved by another instance shows up when the editor or the selector opens")
{
    juce::ScopedJuceInitialiser_GUI gui;
    TempRoot tmp;
    prepareRoot (tmp.dir);
    SwingersProcessor writer (tmp.dir), reader (tmp.dir);

    std::unique_ptr<juce::AudioProcessorEditor> editor (reader.createEditor());
    auto& compact = child<CompactPanel> (*editor, "compactPanel");
    auto& fromBox = child<MapListBox> (compact, "fromBox");
    auto& toBox = child<MapListBox> (compact, "toBox");
    CHECK_FALSE (itemTexts (fromBox).contains ("Fresh Kit"));

    DrumMap fresh;
    fresh.id = "fresh-kit";
    fresh.name = "Fresh Kit";
    fresh.notes = { { 40, "Snare", 38, false } };
    REQUIRE (writer.library().saveMap (fresh).wasOk());
    CHECK_FALSE (itemTexts (fromBox).contains ("Fresh Kit")); // this instance has not looked yet

    // The From / To list reloads right before it shows its items.
    REQUIRE (toBox.beforePopup != nullptr);
    toBox.beforePopup();
    CHECK (itemTexts (fromBox).contains ("Fresh Kit"));
    CHECK (itemTexts (toBox).contains ("Fresh Kit"));

    // A newly opened editor reloads too.
    DrumMap later = fresh;
    later.id = "later-kit";
    later.name = "Later Kit";
    REQUIRE (writer.library().saveMap (later).wasOk());
    editor.reset();
    editor.reset (reader.createEditor());
    CHECK (itemTexts (child<MapListBox> (child<CompactPanel> (*editor, "compactPanel"), "fromBox")).contains ("Later Kit"));
}

TEST_CASE ("the From / To lists follow their own ids when the library reloads behind them")
{
    juce::ScopedJuceInitialiser_GUI gui;
    TempRoot tmp;
    prepareRoot (tmp.dir);
    SwingersProcessor processor (tmp.dir);
    CompactPanel panel (processor);
    auto& fromBox = child<juce::ComboBox> (panel, "fromBox");
    const auto testKitItem = idOfItem (fromBox, "Test Kit");

    // Another instance adds a map that sorts first; this library reloads (e.g. the map editor's New) but the
    // panel has not refilled its lists yet.
    {
        MapLibrary other (tmp.dir);
        DrumMap first;
        first.id = "aaa-kit";
        first.name = "AAA Kit";
        first.notes = { { 40, "Snare", 38, false } };
        REQUIRE (other.saveMap (first).wasOk());
    }
    processor.library().reload();

    fromBox.setSelectedId (testKitItem, juce::sendNotificationSync);
    CHECK (processor.current().from.id == "test-kit");
}

namespace
{
// My Kit as To with exceptions GM -> My Kit; then another instance changes the map and/or the pair file and
// this processor reloads, so the project's copies win (with warnings).
struct CopyWinsFixture
{
    CopyWinsFixture (bool changeMap, bool changePair)
    {
        DrumMap kit;
        kit.id = "my-kit";
        kit.name = "My Kit";
        kit.notes = { { 40, "Snare", 38, false } };
        {
            MapLibrary writer (tmp.dir);
            REQUIRE (writer.saveMap (kit).wasOk());
        }
        processor = std::make_unique<SwingersProcessor> (tmp.dir);
        processor->selectMaps (kGeneralMidiId, "my-kit");
        PairOverrides overrides;
        overrides.fromId = kGeneralMidiId;
        overrides.toId = "my-kit";
        overrides.connect (36, 40);
        processor->applyOverrides (overrides);
        REQUIRE (processor->current().warnings.isEmpty());

        MapLibrary other (tmp.dir);
        if (changeMap)
        {
            kit.notes = { { 41, "Snare", 38, false } };
            REQUIRE (other.saveMap (kit).wasOk());
        }
        if (changePair)
        {
            overrides.connect (36, 50);
            REQUIRE (other.saveOverrides (overrides).wasOk());
        }
        processor->reloadLibrary();
        panel = std::make_unique<CompactPanel> (*processor);
    }

    juce::ScopedJuceInitialiser_GUI gui;
    TempRoot tmp;
    std::unique_ptr<SwingersProcessor> processor;
    std::unique_ptr<CompactPanel> panel;
};
} // namespace

TEST_CASE ("picking the shown map again in the popup switches to this computer's version")
{
    CopyWinsFixture f (true, false);
    REQUIRE (f.processor->current().warnings.size() == 1);
    CHECK (f.processor->current().to.find (40) != nullptr); // the project's copy
    auto& toBox = child<MapListBox> (*f.panel, "toBox");
    CHECK (toBox.getText() == "My Kit"); // the very item is shown

    toBox.pickFromPopup (toBox.getSelectedId());
    CHECK (f.processor->current().warnings.isEmpty());
    CHECK (f.processor->current().to.find (41) != nullptr);
    CHECK (f.processor->current().to.find (40) == nullptr);
}

TEST_CASE ("picking a map again in the popup adopts this computer's pair file")
{
    CopyWinsFixture f (false, true);
    REQUIRE (f.processor->current().warnings.size() == 1);
    CHECK (f.processor->current().overrides.direct.at (36) == 40);
    auto& fromBox = child<MapListBox> (*f.panel, "fromBox");

    int changes = 0;
    f.processor->onStateChanged = [&] { ++changes; };
    fromBox.pickFromPopup (fromBox.getSelectedId());
    pumpMessages(); // an async change notification must not choose a second time
    CHECK (changes == 1);
    CHECK (f.processor->current().warnings.isEmpty());
    CHECK (f.processor->current().overrides.direct.at (36) == 50);
    CHECK (juce::JSON::toString (toJson (f.processor->current().overrides))
           == juce::JSON::toString (toJson (f.processor->library().overrides (kGeneralMidiId, "my-kit"))));
}
