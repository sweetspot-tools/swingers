// Renders the plugin editor to a PNG: swingers_render <output.png> [scale]
// Uses a throw-away user root with one extra map and one broken file, so the lists show both.
#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf (stderr, "usage: swingers_render <output.png> [scale]\n");
        return 2;
    }

    juce::ScopedJuceInitialiser_GUI gui;
    const auto scale = argc > 2 ? juce::String (argv[2]).getFloatValue() : 2.0f;

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
        }
        root.getChildFile ("maps/broken.swmap.json").replaceWithText ("{ not json");

        swingers::SwingersProcessor processor (root);
        // A project whose "To" map is only a copy inside the project: shows the amber warning row.
        {
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

        // Light the LED as it would be mid-note.
        if (auto* e = dynamic_cast<swingers::SwingersEditor*> (editor.get()))
            e->compactPanel().pollActivity (0);
        juce::AudioBuffer<float> audio (0, 16);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 100), 0);
        processor.processBlock (audio, midi);
        if (auto* e = dynamic_cast<swingers::SwingersEditor*> (editor.get()))
            e->compactPanel().pollActivity (1000);

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
