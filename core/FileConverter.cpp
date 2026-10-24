#include "FileConverter.h"

#include "NoteRouter.h"

#include <filesystem>
#include <system_error>

namespace swingers
{
namespace
{
bool isNoteOn (const juce::MidiMessage& m)
{
    return m.isNoteOn (false); // velocity 0 is a note-off
}

bool onProcessedChannel (const juce::MidiMessage& m, int channel)
{
    return channel == 0 || m.getChannel() == channel;
}

void copyTimeFormat (const juce::MidiFile& from, juce::MidiFile& to)
{
    const int format = from.getTimeFormat();

    if (format > 0)
    {
        to.setTicksPerQuarterNote (format);
    }
    else
    {
        // Negative: upper byte = -frames per second, lower byte = ticks per frame.
        to.setSmpteTimeFormat (-(format >> 8), format & 0xff);
    }
}
} // namespace

bool isSameFile (const juce::File& a, const juce::File& b)
{
    if (a == b || a.getLinkedTarget() == b.getLinkedTarget())
        return true;

    // equivalent() needs both files to exist: when the output is not there yet it cannot be the input.
    std::error_code ec;
#if JUCE_WINDOWS
    const std::filesystem::path pa (a.getFullPathName().toWideCharPointer()), pb (b.getFullPathName().toWideCharPointer());
#else
    const std::filesystem::path pa (a.getFullPathName().toStdString()), pb (b.getFullPathName().toStdString());
#endif
    return std::filesystem::equivalent (pa, pb, ec) && ! ec;
}

ConversionReport convertMidiFile (const juce::MidiFile& in, int fileType, const ConversionTable& table,
                                  int channel, juce::MidiFile& out)
{
    juce::ignoreUnused (fileType); // the type only matters when writing

    ConversionReport report;
    juce::MidiFile result; // built aside so that `in` and `out` may be the same object

    copyTimeFormat (in, result);

    for (int t = 0; t < in.getNumTracks(); ++t)
    {
        const auto* source = in.getTrack (t);
        juce::MidiMessageSequence converted;
        NoteRouter router; // fresh per track: held notes never cross tracks
        bool touched = false;

        for (int i = 0; i < source->getNumEvents(); ++i)
        {
            juce::MidiMessage message = source->getEventPointer (i)->message; // keeps the timestamp in ticks
            const bool noteOn = isNoteOn (message);
            const int sourceNote = message.getNoteNumber();
            const bool processed = onProcessedChannel (message, channel);

            switch (router.route (message, table, channel))
            {
                case NoteRouter::Outcome::Dropped:
                {
                    if (noteOn)
                    {
                        const auto route = table.entries[(size_t) sourceNote].route;
                        auto& bucket = route == Route::DroppedByUser ? report.droppedByUser : report.droppedNoMatch;
                        ++bucket[sourceNote];
                        touched = true;
                    }

                    continue;
                }

                case NoteRouter::Outcome::Remapped:
                case NoteRouter::Outcome::Pass:
                    if (noteOn && processed)
                    {
                        ++report.notesConverted;
                        touched = true;
                    }

                    break;
            }

            converted.addEvent (message);
        }

        // No updateMatchedPairs(): it inserts synthetic note-offs when a note retriggers while held (two
        // sources merged onto one target do that), which would change the event count of the file.
        result.addTrack (converted);

        if (touched)
            ++report.tracksTouched;
    }

    int dropped = 0;

    for (const auto& [note, count] : report.droppedNoMatch)
        dropped += count;

    for (const auto& [note, count] : report.droppedByUser)
        dropped += count;

    if (report.notesConverted + dropped == 0)
    {
        report.error = "No notes on the selected channel";
        out.clear();
        return report;
    }

    out = result;
    return report;
}

ConversionReport convertMidiFile (const juce::File& in, const juce::File& out, const ConversionTable& table,
                                  int channel)
{
    ConversionReport report;

    if (isSameFile (in, out))
    {
        report.error = "The output file must be different from the input file";
        return report;
    }

    juce::MidiFile source;
    int fileType = 1;

    {
        juce::FileInputStream stream (in);

        if (! stream.openedOk() || ! source.readFrom (stream, false, &fileType))
        {
            report.error = "Cannot read the MIDI file";
            return report;
        }
    }

    juce::MidiFile converted;
    report = convertMidiFile (source, fileType, table, channel, converted);

    if (! report.ok())
        return report;

    juce::TemporaryFile temp (out);
    bool written = false;

    {
        juce::FileOutputStream stream (temp.getFile());

        if (stream.openedOk())
        {
            stream.setPosition (0);
            stream.truncate();
            written = converted.writeTo (stream, fileType);
            stream.flush();
            written = written && ! stream.getStatus().failed();
        }
    }

    if (! written || ! temp.overwriteTargetFileWithTemporary())
        report.error = "Cannot write the output file";

    return report;
}
} // namespace swingers
