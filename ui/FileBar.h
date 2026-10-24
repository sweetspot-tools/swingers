#pragma once

#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <optional>
#include <vector>

namespace swingers
{
// Standalone only: OPEN MIDI... -> CONVERT -> SAVE... and a report. It covers the whole editor (520 x 330, the
// compact panel is drawn under it) but only its own controls take clicks. The file is read once and kept in
// memory with its type; CONVERT builds the table from the processor's current maps and keeps the result;
// SAVE... writes it. Message thread only.
class FileBar : public juce::Component
{
public:
    static constexpr int kWidth = 520;
    static constexpr int kHeight = 330;

    explicit FileBar (SwingersProcessor&);
    ~FileBar() override;

    // What the buttons do once the file chooser has returned. Public so tests (and the render tool) run the
    // same code. They show their outcome in the report and return false on failure.
    bool loadFile (const juce::File&);
    bool convert();
    bool saveTo (const juce::File&);

    bool hasFile() const { return loaded; }
    bool hasConverted() const { return converted; }
    juce::File sourceFile() const { return source; }
    // <original name without extension>-<To map name, spaces -> '-'>.mid next to the original.
    juce::File suggestedOutput() const;

    // The report as text, one entry per line (what is painted).
    juce::StringArray reportLines() const;

    // Where the wear must stay clear: the boxes, the controls and the report text (editor coordinates).
    static std::vector<juce::Rectangle<int>> wearKeepClear();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Line
    {
        juce::String text;
        bool amber = false;
    };

    void updateButtons();
    void chooseFileToOpen();
    void chooseFileToSave();

    SwingersProcessor& processor;
    juce::TextButton openButton { "Open MIDI..." };
    juce::TextButton convertButton { "Convert" };
    juce::TextButton saveButton { "Save..." };
    juce::Label fileNameLabel;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::MidiFile midi;       // the file as read
    int fileType = 1;
    juce::File source;
    bool loaded = false;

    juce::MidiFile result;     // the converted file
    juce::String resultMapName; // "To" map of that conversion, for the suggested name
    bool converted = false;

    std::vector<Line> summary; // tracks / dropped lines, or the error of a load or convert
    std::optional<Line> status; // "Saved: ..." or the error of a save

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FileBar)
};
} // namespace swingers
