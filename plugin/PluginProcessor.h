#pragma once

#include "LiveRemapper.h"
#include "MapLibrary.h"
#include "PluginState.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <functional>
#include <memory>

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

    // Threading: library(), current(), selectMaps, setChannel, applyOverrides and onStateChanged are
    // message-thread only (onStateChanged is always called there). The audio thread touches only the
    // remapper. getStateInformation/setStateInformation may be called by the host on any thread: they only
    // exchange a serialized snapshot under stateLock_ (never taken by the audio thread); setStateInformation
    // applies the new state on the message thread.
    MapLibrary& library() { return library_; }
    LiveRemapper& remapper() { return remapper_; }
    const ResolvedMaps& current() const { return current_; }
    int channel() const { return state_.channel; } // 0 = all, 1..16

    void selectMaps(const juce::String& fromId, const juce::String& toId);
    void setChannel(int channel);
    void applyOverrides(const PairOverrides&); // saves to the library and rebuilds the table

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

    juce::CriticalSection stateLock_;
    juce::MemoryBlock stateSnapshot_; // guarded by stateLock_
    std::shared_ptr<std::atomic<bool>> alive_ { std::make_shared<std::atomic<bool>>(true) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SwingersProcessor)
};

} // namespace swingers
