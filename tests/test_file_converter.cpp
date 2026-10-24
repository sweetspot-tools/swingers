#include <catch2/catch_test_macros.hpp>

#include "FileConverter.h"

#include <cstring>
#include <utility>
#include <vector>

using namespace swingers;

namespace
{
ConversionTable overrideTable (std::initializer_list<std::pair<int, int>> mappings)
{
    auto table = ConversionTable::identity();

    for (const auto& [from, to] : mappings)
    {
        table.entries[(size_t) from].target = to;
        table.entries[(size_t) from].route = Route::Override;
    }

    return table;
}

bool sameBytes (const juce::MidiMessage& a, const juce::MidiMessage& b)
{
    return a.getRawDataSize() == b.getRawDataSize()
        && std::memcmp (a.getRawData(), b.getRawData(), (size_t) a.getRawDataSize()) == 0;
}

juce::MidiMessage at (juce::MidiMessage m, double tick)
{
    m.setTimeStamp (tick);
    return m;
}

bool writeFile (const juce::File& file, const juce::MidiFile& midi, int fileType)
{
    juce::FileOutputStream stream (file);
    return stream.openedOk() && midi.writeTo (stream, fileType);
}

bool readFile (const juce::File& file, juce::MidiFile& midi, int& fileType)
{
    juce::FileInputStream stream (file);
    return stream.openedOk() && midi.readFrom (stream, false, &fileType);
}

int countNoteEvents (const juce::MidiMessageSequence& seq, int note)
{
    int n = 0;

    for (int i = 0; i < seq.getNumEvents(); ++i)
    {
        const auto& m = seq.getEventPointer (i)->message;

        if ((m.isNoteOn (true) || m.isNoteOff (true)) && m.getNoteNumber() == note)
            ++n;
    }

    return n;
}
} // namespace

TEST_CASE ("keeps every tick and only changes note numbers")
{
    juce::MidiMessageSequence tempoTrack;
    tempoTrack.addEvent (at (juce::MidiMessage::tempoMetaEvent (500000), 0));
    tempoTrack.addEvent (at (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0));
    tempoTrack.addEvent (at (juce::MidiMessage::endOfTrack(), 3840));

    juce::MidiMessageSequence drums;
    drums.addEvent (at (juce::MidiMessage::programChange (10, 5), 0));
    drums.addEvent (at (juce::MidiMessage::controllerEvent (10, 7, 99), 0));

    const double ticks[] = { 0, 1, 959, 1921 };

    for (const double tick : ticks)
    {
        drums.addEvent (at (juce::MidiMessage::noteOn (10, 23, (juce::uint8) 100), tick));
        drums.addEvent (at (juce::MidiMessage::noteOff (10, 23, (juce::uint8) 0), tick + 10));
        drums.addEvent (at (juce::MidiMessage::noteOn (10, 46, (juce::uint8) 90), tick));
        drums.addEvent (at (juce::MidiMessage::noteOff (10, 46, (juce::uint8) 0), tick + 20));
    }

    drums.addEvent (at (juce::MidiMessage::pitchWheel (10, 9000), 500));
    drums.addEvent (at (juce::MidiMessage::endOfTrack(), 3840));

    juce::MidiFile in;
    in.setTicksPerQuarterNote (960);
    in.addTrack (tempoTrack);
    in.addTrack (drums);

    juce::MidiFile out;
    const auto report = convertMidiFile (in, 1, overrideTable ({ { 23, 26 }, { 46, 26 } }), 10, out);

    REQUIRE (report.ok());
    REQUIRE (out.getTimeFormat() == 960);
    REQUIRE (out.getNumTracks() == 2);
    CHECK (report.tracksTouched == 1);
    CHECK (report.notesConverted == 8);
    CHECK (report.droppedNoMatch.empty());
    CHECK (report.droppedByUser.empty());

    for (int t = 0; t < 2; ++t)
    {
        const auto* a = in.getTrack (t);
        const auto* b = out.getTrack (t);
        REQUIRE (a->getNumEvents() == b->getNumEvents());

        for (int i = 0; i < a->getNumEvents(); ++i)
        {
            const auto& ma = a->getEventPointer (i)->message;
            const auto& mb = b->getEventPointer (i)->message;

            CHECK (juce::exactlyEqual (ma.getTimeStamp(), mb.getTimeStamp()));

            if (ma.isNoteOnOrOff())
            {
                CHECK (mb.getNoteNumber() == 26);
                CHECK (mb.getChannel() == ma.getChannel());
                CHECK (mb.getVelocity() == ma.getVelocity());
                CHECK (mb.getRawData()[0] == ma.getRawData()[0]);
            }
            else
            {
                CHECK (sameBytes (ma, mb));
            }
        }
    }
}

TEST_CASE ("keeps an SMPTE time format")
{
    juce::MidiMessageSequence track;
    track.addEvent (at (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 5));
    track.addEvent (at (juce::MidiMessage::noteOff (1, 36, (juce::uint8) 0), 9));

    juce::MidiFile in;
    in.setSmpteTimeFormat (25, 40);
    in.addTrack (track);

    juce::MidiFile out;
    REQUIRE (convertMidiFile (in, 0, ConversionTable::identity(), 0, out).ok());
    CHECK (out.getTimeFormat() == in.getTimeFormat());
    CHECK (out.getTimeFormat() < 0);
}

TEST_CASE ("format 0 stays format 0")
{
    juce::MidiMessageSequence track;
    track.addEvent (at (juce::MidiMessage::noteOn (1, 23, (juce::uint8) 100), 0));
    track.addEvent (at (juce::MidiMessage::noteOff (1, 23, (juce::uint8) 0), 100));
    track.addEvent (at (juce::MidiMessage::noteOn (10, 23, (juce::uint8) 100), 120));
    track.addEvent (at (juce::MidiMessage::noteOff (10, 23, (juce::uint8) 0), 220));
    track.addEvent (at (juce::MidiMessage::noteOn (1, 46, (juce::uint8) 80), 240));
    track.addEvent (at (juce::MidiMessage::noteOff (1, 46, (juce::uint8) 0), 300));

    juce::MidiFile in;
    in.setTicksPerQuarterNote (96);
    in.addTrack (track);

    juce::TemporaryFile inFile (".mid"), outFile (".mid");
    REQUIRE (writeFile (inFile.getFile(), in, 0));

    const auto report = convertMidiFile (inFile.getFile(), outFile.getFile(), overrideTable ({ { 23, 26 } }), 10);
    REQUIRE (report.ok());
    CHECK (report.notesConverted == 1);

    juce::MidiFile result;
    int fileType = -1;
    REQUIRE (readFile (outFile.getFile(), result, fileType));
    CHECK (fileType == 0);
    CHECK (result.getTimeFormat() == 96);
    REQUIRE (result.getNumTracks() == 1);

    const auto* seq = result.getTrack (0);
    std::vector<std::pair<int, int>> notes; // (channel, note) in order

    for (int i = 0; i < seq->getNumEvents(); ++i)
        if (const auto& m = seq->getEventPointer (i)->message; m.isNoteOnOrOff())
            notes.emplace_back (m.getChannel(), m.getNoteNumber());

    const std::vector<std::pair<int, int>> expected { { 1, 23 }, { 1, 23 }, { 10, 26 }, { 10, 26 }, { 1, 46 }, { 1, 46 } };
    CHECK (notes == expected);
    CHECK (juce::exactlyEqual (seq->getEventPointer (2)->message.getTimeStamp(), 120.0));
    CHECK (juce::exactlyEqual (seq->getEventPointer (3)->message.getTimeStamp(), 220.0));
}

TEST_CASE ("drops unmatched notes and reports them")
{
    auto table = overrideTable ({ { 23, 26 } });
    table.entries[70].route = Route::NoMatch;
    table.entries[70].target = -1;
    table.entries[61].route = Route::DroppedByUser;

    juce::MidiMessageSequence track;
    double tick = 0;

    for (const int note : { 23, 70, 70, 61, 23, 70, 61, 23 })
    {
        track.addEvent (at (juce::MidiMessage::noteOn (10, note, (juce::uint8) 100), tick));
        track.addEvent (at (juce::MidiMessage::noteOff (10, note, (juce::uint8) 0), tick + 30));
        tick += 100;
    }

    // A note-on with velocity 0 is a note-off: it is neither converted nor counted as dropped.
    track.addEvent (at (juce::MidiMessage::noteOn (10, 70, (juce::uint8) 100), tick));
    track.addEvent (at (juce::MidiMessage (0x90 | 9, 70, 0, tick + 10), tick + 10));

    juce::MidiMessageSequence other;
    other.addEvent (at (juce::MidiMessage::tempoMetaEvent (500000), 0));

    juce::MidiFile in;
    in.setTicksPerQuarterNote (480);
    in.addTrack (other);
    in.addTrack (track);

    juce::MidiFile out;
    const auto report = convertMidiFile (in, 1, table, 10, out);

    REQUIRE (report.ok());
    CHECK (report.droppedNoMatch == std::map<int, int> { { 70, 4 } });
    CHECK (report.droppedByUser == std::map<int, int> { { 61, 2 } });
    CHECK (report.notesConverted == 3);
    CHECK (report.tracksTouched == 1);

    REQUIRE (out.getNumTracks() == 2);
    CHECK (countNoteEvents (*out.getTrack (1), 70) == 0);
    CHECK (countNoteEvents (*out.getTrack (1), 61) == 0);
    CHECK (countNoteEvents (*out.getTrack (1), 26) == 6);
    CHECK (out.getTrack (0)->getNumEvents() == 1);
}

TEST_CASE ("refuses to overwrite the input")
{
    juce::MidiMessageSequence track;
    track.addEvent (at (juce::MidiMessage::noteOn (10, 23, (juce::uint8) 100), 0));
    track.addEvent (at (juce::MidiMessage::noteOff (10, 23, (juce::uint8) 0), 50));

    juce::MidiFile in;
    in.setTicksPerQuarterNote (480);
    in.addTrack (track);

    juce::TemporaryFile file (".mid");
    REQUIRE (writeFile (file.getFile(), in, 1));
    const auto before = file.getFile().getSize();

    const auto report = convertMidiFile (file.getFile(), file.getFile(), ConversionTable::identity(), 0);

    CHECK (! report.ok());
    CHECK (report.error == "The output file must be different from the input file");
    CHECK (file.getFile().getSize() == before);
}

TEST_CASE ("refuses to overwrite the input through a symlinked folder")
{
    juce::MidiMessageSequence track;
    track.addEvent (at (juce::MidiMessage::noteOn (10, 23, (juce::uint8) 100), 0));
    track.addEvent (at (juce::MidiMessage::noteOff (10, 23, (juce::uint8) 0), 50));

    juce::MidiFile in;
    in.setTicksPerQuarterNote (480);
    in.addTrack (track);

    const auto base = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("swingers-link-" + juce::Uuid().toString());
    const auto real = base.getChildFile ("real");
    REQUIRE (real.createDirectory());
    const auto link = base.getChildFile ("link");

    if (! real.createSymbolicLink (link, true))
    {
        base.deleteRecursively();
        SKIP ("symbolic links are not available here");
    }

    const auto input = real.getChildFile ("in.mid");
    REQUIRE (writeFile (input, in, 1));
    const auto before = input.getSize();

    const auto report = convertMidiFile (input, link.getChildFile ("in.mid"), ConversionTable::identity(), 0);
    CHECK (report.error == "The output file must be different from the input file");
    CHECK (input.getSize() == before);
    base.deleteRecursively();
}

TEST_CASE ("refuses a file with no notes on the channel")
{
    juce::MidiMessageSequence track;
    track.addEvent (at (juce::MidiMessage::noteOn (1, 23, (juce::uint8) 100), 0));
    track.addEvent (at (juce::MidiMessage::noteOff (1, 23, (juce::uint8) 0), 50));

    juce::MidiFile in;
    in.setTicksPerQuarterNote (480);
    in.addTrack (track);

    juce::TemporaryFile inFile (".mid"), outFile (".mid");
    REQUIRE (writeFile (inFile.getFile(), in, 1));

    const auto report = convertMidiFile (inFile.getFile(), outFile.getFile(), ConversionTable::identity(), 10);

    CHECK (! report.ok());
    CHECK (report.error == "No notes on the selected channel");
    CHECK (! outFile.getFile().exists());
}

TEST_CASE ("reports an unreadable input file")
{
    juce::TemporaryFile inFile (".mid"), outFile (".mid");
    REQUIRE (inFile.getFile().replaceWithText ("this is not a MIDI file"));

    const auto report = convertMidiFile (inFile.getFile(), outFile.getFile(), ConversionTable::identity(), 0);

    CHECK (report.error == "Cannot read the MIDI file");
    CHECK (! outFile.getFile().exists());

    const auto missing = convertMidiFile (inFile.getFile().getSiblingFile ("does-not-exist.mid"), outFile.getFile(),
                                          ConversionTable::identity(), 0);
    CHECK (missing.error == "Cannot read the MIDI file");
}
