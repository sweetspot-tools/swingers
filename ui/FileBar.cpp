#include "FileBar.h"

#include "ConversionTable.h"
#include "FileConverter.h"
#include "RackLookAndFeel.h"

#include <map>

namespace swingers
{
namespace
{
// Geometry from the approved preview (520 x 330). The FILE box sits 7 px lower than the preview so that it
// does not cover the warnings row of the compact panel.
juce::Rectangle<int> fileBoxArea()    { return { 14, 131, 492, 46 }; }
juce::Rectangle<int> openArea()       { return { 26, 141, 120, 26 }; }
juce::Rectangle<int> fileNameArea()   { return { 156, 141, 136, 26 }; }
juce::Rectangle<int> convertArea()    { return { 300, 141, 90, 26 }; }
juce::Rectangle<int> saveArea()       { return { 400, 141, 94, 26 }; }
juce::Rectangle<int> reportBoxArea()  { return { 14, 200, 492, 118 }; }
juce::Rectangle<int> reportLineArea (int i) { return { 28, 214 + 22 * i, 464, 18 }; }

constexpr int kMaxReportLines = 4;

const juce::String kDot = juce::String::fromUTF8 (" \xc2\xb7 ");
const juce::String kTimes = juce::String::fromUTF8 ("\xc3\x97");

juce::String describeDropped (const std::map<int, int>& dropped, const DrumMap& from)
{
    juce::StringArray parts;
    for (const auto& [note, count] : dropped)
    {
        juce::String part (note);
        if (const auto* entry = from.find (note); entry != nullptr && entry->name.isNotEmpty())
            part << ' ' << entry->name;
        part << ' ' << kTimes << count;
        parts.add (part);
    }
    return parts.joinIntoString (", ");
}

juce::String legalName (const juce::String& stem, const juce::String& mapName)
{
    return juce::File::createLegalFileName (stem + "-" + mapName.replaceCharacter (' ', '-'));
}
} // namespace

FileBar::FileBar (SwingersProcessor& p) : processor (p)
{
    setInterceptsMouseClicks (false, true);

    openButton.setButtonText (juce::String::fromUTF8 ("Open MIDI\xe2\x80\xa6"));
    saveButton.setButtonText (juce::String::fromUTF8 ("Save\xe2\x80\xa6"));
    openButton.setComponentID ("openMidi");
    convertButton.setComponentID ("convertMidi");
    saveButton.setComponentID ("saveMidi");
    fileNameLabel.setComponentID ("fileName");

    openButton.setTooltip ("Choose a MIDI file (.mid)");
    convertButton.setTooltip ("Convert the file with the current From / To maps");
    saveButton.setTooltip ("Save the converted file");
    // The look-and-feel gives a toggle that is on the amber face: CONVERT lights when it is the next action.
    convertButton.setClickingTogglesState (true);

    openButton.onClick = [this] { chooseFileToOpen(); };
    convertButton.onClick = [this] { convert(); };
    saveButton.onClick = [this] { chooseFileToSave(); };

    fileNameLabel.setFont (RackLookAndFeel::silkscreenFont (11.0f));
    fileNameLabel.setMinimumHorizontalScale (1.0f); // clip with an ellipsis, do not squeeze

    addAndMakeVisible (openButton);
    addAndMakeVisible (convertButton);
    addAndMakeVisible (saveButton);
    addAndMakeVisible (fileNameLabel);
    setSize (kWidth, kHeight);
    updateButtons();
}

FileBar::~FileBar() = default;

void FileBar::updateButtons()
{
    convertButton.setEnabled (loaded);
    saveButton.setEnabled (converted);
    convertButton.setToggleState (loaded && ! converted, juce::dontSendNotification);

    fileNameLabel.setText (loaded ? source.getFileName() : juce::String ("no file"), juce::dontSendNotification);
    fileNameLabel.setColour (juce::Label::textColourId, loaded ? colours::white : colours::silkscreen.withAlpha (0.6f));
    fileNameLabel.setTooltip (loaded ? source.getFullPathName() : juce::String());
    repaint();
}

bool FileBar::loadFile (const juce::File& file)
{
    juce::MidiFile read;
    int type = 1;
    bool ok = false;
    {
        juce::FileInputStream stream (file);
        ok = stream.openedOk() && read.readFrom (stream, false, &type);
    }

    if (! ok)
    {
        // Whatever was loaded (and converted) stays; the error is shown under it.
        status = Line { "Cannot read the MIDI file", true };
        updateButtons();
        return false;
    }

    midi = read;
    fileType = type;
    source = file;
    loaded = true;
    converted = false;
    result.clear();
    status.reset();
    summary.clear();
    updateButtons();
    return true;
}

juce::String FileBar::inputsSignature() const
{
    const auto& maps = processor.current();
    return juce::JSON::toString (toJson (maps.from), true) + '\n' + juce::JSON::toString (toJson (maps.to), true)
         + '\n' + juce::JSON::toString (toJson (maps.overrides), true) + '\n' + juce::String (processor.channel());
}

void FileBar::invalidateResult()
{
    if (! converted)
        return;

    converted = false;
    result.clear();
    status.reset();
    summary.clear();
    summary.push_back ({ "Maps changed: convert again", false });
    updateButtons();
}

void FileBar::invalidateIfInputsChanged()
{
    if (converted && inputsSignature() != resultInputs)
        invalidateResult();
}

bool FileBar::convert()
{
    if (! loaded)
        return false;

    const auto& maps = processor.current();
    const auto table = ConversionTable::build (maps.from, maps.to, &maps.overrides);

    juce::MidiFile out;
    const auto report = convertMidiFile (midi, fileType, table, processor.channel(), out);

    status.reset();
    summary.clear();
    converted = false;
    result.clear();

    if (! report.ok())
    {
        summary.push_back ({ report.error, true });
        updateButtons();
        return false;
    }

    result = out;
    converted = true;
    resultInputs = inputsSignature();
    resultMapName = maps.to.name.isNotEmpty() ? maps.to.name : maps.to.id;

    summary.push_back ({ "Tracks: " + juce::String (report.tracksTouched) + kDot + "notes converted: "
                             + juce::String (report.notesConverted),
                         false });

    if (report.droppedNoMatch.empty() && report.droppedByUser.empty())
    {
        summary.push_back ({ "Dropped: none", false });
    }
    else
    {
        const auto line = [&] (const juce::String& label, const std::map<int, int>& dropped) {
            if (dropped.empty())
                summary.push_back ({ label + ": none", false });
            else
                summary.push_back ({ label + ": " + describeDropped (dropped, maps.from), true });
        };
        line ("Dropped (no match)", report.droppedNoMatch);
        line ("Dropped (disconnected)", report.droppedByUser);
    }

    updateButtons();
    return true;
}

juce::File FileBar::suggestedOutput() const
{
    if (! loaded)
        return {};
    return source.getSiblingFile (legalName (source.getFileNameWithoutExtension(), resultMapName) + ".mid");
}

bool FileBar::saveTo (const juce::File& chosen)
{
    const auto file = chosen.getFileExtension().isEmpty() ? chosen.withFileExtension ("mid") : chosen;

    if (! converted)
    {
        // A save dialog can come back after the maps changed (or the file was replaced).
        status = Line { "Nothing to save: convert again", true };
        repaint();
        return false;
    }

    status.reset();

    if (isSameFile (file, source))
    {
        status = Line { "The output file must be different from the input file", true };
        repaint();
        return false;
    }

    juce::TemporaryFile temp (file);
    bool written = false;
    {
        juce::FileOutputStream stream (temp.getFile());
        if (stream.openedOk())
        {
            stream.setPosition (0);
            stream.truncate();
            written = result.writeTo (stream, fileType);
            stream.flush();
            written = written && ! stream.getStatus().failed();
        }
    }

    if (! written || ! temp.overwriteTargetFileWithTemporary())
    {
        status = Line { "Cannot write the output file", true };
        repaint();
        return false;
    }

    status = Line { "Saved: " + file.getFileName(), false };
    repaint();
    return true;
}

void FileBar::chooseFileToOpen()
{
    const auto start = loaded ? source : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    chooser = std::make_unique<juce::FileChooser> ("Open MIDI file", start, "*.mid;*.midi");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe = juce::Component::SafePointer<FileBar> (this)] (const juce::FileChooser& fc) {
                              const auto file = fc.getResult();
                              if (safe != nullptr && file != juce::File())
                                  safe->loadFile (file);
                          });
}

void FileBar::chooseFileToSave()
{
    if (! converted)
        return;

    chooser = std::make_unique<juce::FileChooser> ("Save converted MIDI file", suggestedOutput(), "*.mid;*.midi");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe = juce::Component::SafePointer<FileBar> (this)] (const juce::FileChooser& fc) {
                              const auto file = fc.getResult();
                              if (safe != nullptr && file != juce::File())
                                  safe->saveTo (file);
                          });
}

juce::StringArray FileBar::reportLines() const
{
    juce::StringArray lines;
    for (const auto& line : summary)
        lines.add (line.text);
    if (status)
        lines.add (status->text);
    return lines;
}

std::vector<juce::Rectangle<int>> FileBar::wearKeepClear()
{
    return { fileBoxArea().expanded (2), reportBoxArea().expanded (2) };
}

void FileBar::paint (juce::Graphics& g)
{
    using Side = RackLookAndFeel::TitleSide;
    RackLookAndFeel::drawBoxWithTitle (g, fileBoxArea(), "File", Side::bottom);
    RackLookAndFeel::drawBoxWithTitle (g, reportBoxArea(), "Report", Side::top);

    g.setFont (RackLookAndFeel::silkscreenFont (12.0f));
    int i = 0;
    const auto drawLine = [&] (const Line& line) {
        if (i >= kMaxReportLines)
            return;
        g.setColour (line.amber ? colours::amber : colours::silkscreen);
        g.drawText (line.text, reportLineArea (i++), juce::Justification::centredLeft, true);
    };
    for (const auto& line : summary)
        drawLine (line);
    if (status)
        drawLine (*status);
}

void FileBar::resized()
{
    openButton.setBounds (openArea());
    fileNameLabel.setBounds (fileNameArea());
    convertButton.setBounds (convertArea());
    saveButton.setBounds (saveArea());
}
} // namespace swingers
