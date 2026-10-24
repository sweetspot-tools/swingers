#pragma once

#include "LiveRemapper.h"
#include "MapLibrary.h"
#include "PluginState.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <functional>
#include <memory>
#include <utility>
#include <optional>

namespace swingers
{

// MIDI effect: no audio buses, MIDI in and out.
class SwingersProcessor : public juce::AudioProcessor
{
public:
    explicit SwingersProcessor (juce::File userRoot = MapLibrary::defaultUserRoot());
    ~SwingersProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    using juce::AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    // Host bypass: notes go out unchanged, but the offs of notes held from before go to their remapped target
    // (JUCE's default would pass the MIDI raw and leave those notes hanging).
    using juce::AudioProcessor::processBlockBypassed;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Threading: library(), current(), wearSeed, selectMaps, chooseMap, useLibraryVersionOf, reloadLibrary,
    // setChannel, applyOverrides and onStateChanged are message-thread only (onStateChanged is always called
    // there). The audio thread touches only the remapper. getStateInformation/setStateInformation may be called by the host on any thread: they only
    // exchange a serialized snapshot under stateLock_ (never taken by the audio thread); setStateInformation
    // applies the new state on the message thread.
    MapLibrary& library() { return library_; }
    LiveRemapper& remapper() { return remapper_; }
    const ResolvedMaps& current() const { return current_; }
    int channel() const { return state_.channel; } // 0 = all, 1..16

    // Seed of the panel wear, saved with the project. Created at random on first use (never 0).
    uint32_t wearSeed();

    // Programmatic choice of both maps: a project copy with the same id is kept (it wins over a different
    // library version, see resolve()).
    void selectMaps(const juce::String& fromId, const juce::String& toId);

    // The user picked `id` in the From or To selector: that side's project copy and the project's copy of the
    // exceptions are dropped, so this computer's map and pair file (if any) are used.
    enum class Side { from, to };
    void chooseMap(Side side, const juce::String& id);

    // The map editor saved `id`: the sides that use it drop their project copy and take the saved version.
    void useLibraryVersionOf(const juce::String& id);

    // Reloads the library from disk (other instances may have saved maps) and resolves the maps again.
    // Invalidates pointers into library(): callers copy what they keep.
    void reloadLibrary();
    void setChannel(int channel);
    void applyOverrides(const PairOverrides&); // saves to the library and rebuilds the table

    // Unsaved edits of the map editor that survive the editor window being closed (the host may close and
    // reopen it at any time). Kept in memory only: not part of the project state and not saved with it.
    // The editor stores the draft when it is destroyed with unsaved edits and takes it back when it is
    // created again. Message thread only.
    struct MapDraft
    {
        DrumMap map;
        bool isNew = false; // not in the library yet (a new or duplicated map)
    };
    void setMapDraft(MapDraft draft) { mapDraft_ = std::move(draft); }
    std::optional<MapDraft> takeMapDraft() { return std::exchange(mapDraft_, std::nullopt); }
    bool hasMapDraft() const { return mapDraft_.has_value(); }

    std::function<void()> onStateChanged; // editor refresh, called on the message thread

private:
    // Resolves the maps, rebuilds the table and hands it to the remapper. `forcedOverrides` replaces the
    // resolved ones (used when saving them to the library failed).
    void rebuild(const PairOverrides* forcedOverrides = nullptr, const juce::String& extraWarning = {});
    void notifyChanged();

    void applyLoadedState(PluginState loaded);
    void refreshSnapshot();

    MapLibrary library_;
    LiveRemapper remapper_;
    PluginState state_;
    ResolvedMaps current_;
    std::optional<MapDraft> mapDraft_; // message thread only

    juce::CriticalSection stateLock_;
    juce::MemoryBlock stateSnapshot_; // guarded by stateLock_
    std::shared_ptr<std::atomic<bool>> alive_ { std::make_shared<std::atomic<bool>>(true) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SwingersProcessor)
};

} // namespace swingers
