// Renders the plugin editor to a PNG:
//   swingers_render <output.png> [scale] [--with-warning] [--patchbay [--all-notes] [--lit]] [--map-editor] [--standalone]
// Uses a throw-away user root with extra maps and one broken file, so the lists show both.
// By default the compact panel shows General MIDI -> Studio Kit, both on this "computer" (no warning row);
// --with-warning loads a project whose To map exists only as the project's copy, so the amber warning row shows.
// --patchbay renders the "Edit mapping" view instead, with the preview's example: two user maps, one direct
// cable (40 -> 33) and one disconnected note (37); --all-notes shows all 128 rows, --lit lights note 36.
// --map-editor renders "Edit maps" on the preview's Metal Kit.
// --standalone renders the standalone layout (compact panel, file bar and report) after converting a generated
// MIDI file with the preview's example: 412 notes converted, Splash FX (61) dropped three times, then saved.
#include "FileBar.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace
{
swingers::DrumMap makeMap (const char* id, const char* name, std::vector<swingers::MapNote> notes)
{
    swingers::DrumMap map;
    map.id = id;
    map.name = name;
    map.library = "Demo";
    map.notes = std::move (notes);
    return map;
}

// The example of docs/mockups/swingers_preview.html (Edit mapping).
void writePatchbayMaps (swingers::MapLibrary& writer)
{
    writer.saveMap (makeMap ("studio-kit", "Studio Kit",
                             { { 23, "HH Open 2", 46, false }, { 36, "Kick", 36, false }, { 37, "Snare Rim", 38, false },
                               { 38, "Snare", 38, false }, { 40, "Snare Flam", swingers::kNoGm, false },
                               { 42, "HH Closed", 42, false }, { 44, "HH Pedal", 44, false }, { 46, "HH Open", 46, false },
                               { 49, "Crash L", 49, false }, { 51, "Ride", 51, false }, { 53, "Ride Bell", 53, false },
                               { 57, "Crash R", 57, false }, { 60, "China", 52, false },
                               { 61, "Splash FX", swingers::kNoGm, false } }));
    writer.saveMap (makeMap ("metal-kit", "Metal Kit",
                             { { 26, "HH Open", 46, false }, { 33, "Snare Flam", swingers::kNoGm, false },
                               { 36, "Kick R", 36, false }, { 38, "Snare Center", 38, true },
                               { 40, "Snare Rimshot", 38, false }, { 42, "HH Closed", 42, false },
                               { 44, "HH Pedal", 44, false }, { 49, "Crash 1", 49, false }, { 51, "Ride Tip", 51, false },
                               { 52, "China", 52, false }, { 53, "Ride Bell", 53, false },
                               { 57, "Crash 2", 57, false } }));
}

// A one-track drum file named like the preview's: 412 notes the pair converts and three Splash FX (61).
juce::File writeExampleMidi (const juce::File& folder)
{
    juce::MidiMessageSequence track;
    const int notes[] = { 36, 38, 42, 46 };
    for (int i = 0; i < 412; ++i)
    {
        auto on = juce::MidiMessage::noteOn (10, notes[i % 4], (juce::uint8) 100);
        on.setTimeStamp (i * 240.0);
        auto off = juce::MidiMessage::noteOff (10, notes[i % 4]);
        off.setTimeStamp (i * 240.0 + 100.0);
        track.addEvent (on);
        track.addEvent (off);
    }
    for (int i = 0; i < 3; ++i)
    {
        auto on = juce::MidiMessage::noteOn (10, 61, (juce::uint8) 100);
        on.setTimeStamp (50.0 + i * 960.0);
        auto off = juce::MidiMessage::noteOff (10, 61);
        off.setTimeStamp (90.0 + i * 960.0);
        track.addEvent (on);
        track.addEvent (off);
    }
    juce::MidiFile file;
    file.setTicksPerQuarterNote (480);
    file.addTrack (track);

    const auto path = folder.getChildFile ("groove_verse.mid");
    juce::FileOutputStream stream (path);
    file.writeTo (stream, 1);
    return path;
}
} // namespace

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf (stderr, "usage: swingers_render <output.png> [scale] [--with-warning] [--patchbay [--all-notes] [--lit]] [--map-editor] [--standalone]\n");
        return 2;
    }

    juce::ScopedJuceInitialiser_GUI gui;
    float scale = 2.0f;
    bool patchbay = false, allNotes = false, lit = false, mapEditor = false, standalone = false, withWarning = false;
    for (int i = 2; i < argc; ++i)
    {
        const juce::String arg (argv[i]);
        if (arg == "--patchbay")
            patchbay = true;
        else if (arg == "--all-notes")
            allNotes = true;
        else if (arg == "--lit")
            lit = true;
        else if (arg == "--map-editor")
            mapEditor = true;
        else if (arg == "--standalone")
            standalone = true;
        else if (arg == "--with-warning")
            withWarning = true;
        else
            scale = arg.getFloatValue();
    }

    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("swingers-render-" + juce::Uuid().toString());
    int exitCode = 0;
    {
        swingers::DrumMap map;
        map.id = "my-kit";
        map.name = "My Kit";
        map.library = "Demo";
        map.notes = { { 36, "Kick", 36, true } };
        {
            swingers::MapLibrary writer (root);
            writer.saveMap (map);
            if (patchbay || mapEditor || standalone || ! withWarning)
                writePatchbayMaps (writer);
        }
        root.getChildFile ("maps/broken.swmap.json").replaceWithText ("{ not json");

        swingers::SwingersProcessor processor (root);
        if (mapEditor || standalone)
        {
            processor.selectMaps ("studio-kit", "metal-kit");
        }
        else if (patchbay)
        {
            processor.selectMaps ("studio-kit", "metal-kit");
            swingers::PairOverrides overrides;
            overrides.fromId = "studio-kit";
            overrides.toId = "metal-kit";
            overrides.connect (40, 33);
            overrides.drop (37);
            processor.applyOverrides (overrides);
        }
        else if (! withWarning)
        {
            processor.selectMaps (swingers::kGeneralMidiId, "studio-kit");
        }
        else
        {
            // A project whose "To" map is only a copy inside the project: shows the amber warning row.
            swingers::PluginState state;
            state.toId = "studio-kit";
            swingers::DrumMap copy = map;
            copy.id = "studio-kit";
            copy.name = "Studio Kit";
            state.toCopy = copy;
            state.channel = 0;
            const auto block = swingers::serialize (state);
            processor.setStateInformation (block.getData(), (int) block.getSize());
        }
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

        auto* swingersEditor = dynamic_cast<swingers::SwingersEditor*> (editor.get());
        juce::AudioBuffer<float> audio (0, 16);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 100), 0);

        if (standalone && swingersEditor != nullptr)
        {
            swingersEditor->setFileBarShown (true);
            auto& bar = swingersEditor->fileBarView();
            const auto input = writeExampleMidi (root);
            if (! bar.loadFile (input) || ! bar.convert() || ! bar.saveTo (bar.suggestedOutput()))
            {
                std::fprintf (stderr, "example conversion failed\n");
                exitCode = 1;
            }
            swingersEditor->compactPanel().pollActivity (0);
        }
        else if (mapEditor && swingersEditor != nullptr)
        {
            swingersEditor->showMapEditor (true);
        }
        else if (patchbay && swingersEditor != nullptr)
        {
            swingersEditor->showPatchbay (true);
            swingersEditor->patchbayView().setAllNotes (allNotes);
            if (lit)
            {
                processor.processBlock (audio, midi);
                swingersEditor->patchbayView().pollHits (1000);
            }
        }
        else if (swingersEditor != nullptr)
        {
            // Light the LED as it would be mid-note.
            swingersEditor->compactPanel().pollActivity (0);
            processor.processBlock (audio, midi);
            swingersEditor->compactPanel().pollActivity (1000);
        }

        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);
        const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
        out.deleteFile();
        juce::FileOutputStream stream (out);
        juce::PNGImageFormat png;
        if (! stream.openedOk() || ! png.writeImageToStream (image, stream))
        {
            std::fprintf (stderr, "could not write %s\n", out.getFullPathName().toRawUTF8());
            exitCode = 1;
        }
    }
    root.deleteRecursively();
    return exitCode;
}
