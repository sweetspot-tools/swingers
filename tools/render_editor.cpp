// Renders the plugin editor to a PNG: swingers_render <output.png> [scale] [--patchbay [--all-notes] [--lit]]
// Uses a throw-away user root with one extra map and one broken file, so the lists show both.
// --patchbay renders the "Edit mapping" view instead, with the preview's example: two user maps, one direct
// cable (40 -> 33) and one disconnected note (37); --all-notes shows all 128 rows, --lit lights note 36.
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
} // namespace

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf (stderr, "usage: swingers_render <output.png> [scale] [--patchbay [--all-notes] [--lit]]\n");
        return 2;
    }

    juce::ScopedJuceInitialiser_GUI gui;
    float scale = 2.0f;
    bool patchbay = false, allNotes = false, lit = false;
    for (int i = 2; i < argc; ++i)
    {
        const juce::String arg (argv[i]);
        if (arg == "--patchbay")
            patchbay = true;
        else if (arg == "--all-notes")
            allNotes = true;
        else if (arg == "--lit")
            lit = true;
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
            if (patchbay)
                writePatchbayMaps (writer);
        }
        root.getChildFile ("maps/broken.swmap.json").replaceWithText ("{ not json");

        swingers::SwingersProcessor processor (root);
        if (patchbay)
        {
            processor.selectMaps ("studio-kit", "metal-kit");
            swingers::PairOverrides overrides;
            overrides.fromId = "studio-kit";
            overrides.toId = "metal-kit";
            overrides.connect (40, 33);
            overrides.drop (37);
            processor.applyOverrides (overrides);
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

        if (patchbay && swingersEditor != nullptr)
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
