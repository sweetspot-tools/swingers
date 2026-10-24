#include "PluginProcessor.h"

namespace swingers
{

SwingersProcessor::SwingersProcessor(juce::File userRoot)
    : juce::AudioProcessor(BusesProperties()), library_(std::move(userRoot))
{
    rebuild();
}

SwingersProcessor::~SwingersProcessor()
{
    alive_->store(false);
}

void SwingersProcessor::refreshSnapshot()
{
    auto block = serialize(state_);
    const juce::ScopedLock lock(stateLock_);
    stateSnapshot_ = std::move(block);
}

void SwingersProcessor::rebuild(const PairOverrides* forcedOverrides, const juce::String& extraWarning)
{
    current_ = resolve(state_, library_);
    if (forcedOverrides != nullptr)
        current_.overrides = *forcedOverrides;
    if (extraWarning.isNotEmpty())
        current_.warnings.add(extraWarning);

    // The project keeps copies of what is in use now (only of the maps actually chosen by id).
    state_.fromCopy.reset();
    state_.toCopy.reset();
    if (current_.from.id == state_.fromId)
        state_.fromCopy = current_.from;
    if (current_.to.id == state_.toId)
        state_.toCopy = current_.to;
    state_.overridesCopy = current_.overrides;

    remapper_.setTable(ConversionTable::build(current_.from, current_.to, &current_.overrides));
    remapper_.setChannel(state_.channel);
    refreshSnapshot();
}

void SwingersProcessor::notifyChanged()
{
    if (onStateChanged)
        onStateChanged();
}

void SwingersProcessor::selectMaps(const juce::String& fromId, const juce::String& toId)
{
    state_.fromId = fromId;
    state_.toId = toId;
    rebuild();
    notifyChanged();
}

void SwingersProcessor::setChannel(int channel)
{
    state_.channel = juce::jlimit(0, 16, channel);
    rebuild();
    notifyChanged();
}

void SwingersProcessor::applyOverrides(const PairOverrides& overrides)
{
    const auto saved = library_.saveOverrides(overrides);
    state_.overridesCopy = overrides;

    if (saved.wasOk())
    {
        rebuild();
    }
    else
    {
        const auto message = "Could not save the exceptions: " + saved.getErrorMessage();
        // Keep the in-memory exceptions only when they belong to the pair in use (same check as resolve).
        const bool samePair = overrides.fromId == current_.from.id && overrides.toId == current_.to.id;
        rebuild(samePair ? &overrides : nullptr, message);
    }

    notifyChanged();
}

void SwingersProcessor::prepareToPlay(double, int) {}
void SwingersProcessor::releaseResources() {}

void SwingersProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();
    remapper_.process(midi);
}

juce::AudioProcessorEditor* SwingersProcessor::createEditor() { return nullptr; }
bool SwingersProcessor::hasEditor() const { return false; }

const juce::String SwingersProcessor::getName() const { return "Swingers"; }
bool SwingersProcessor::acceptsMidi() const { return true; }
bool SwingersProcessor::producesMidi() const { return true; }
bool SwingersProcessor::isMidiEffect() const { return true; }
double SwingersProcessor::getTailLengthSeconds() const { return 0.0; }

int SwingersProcessor::getNumPrograms() { return 1; }
int SwingersProcessor::getCurrentProgram() { return 0; }
void SwingersProcessor::setCurrentProgram(int) {}
const juce::String SwingersProcessor::getProgramName(int) { return {}; }
void SwingersProcessor::changeProgramName(int, const juce::String&) {}

void SwingersProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    const juce::ScopedLock lock(stateLock_);
    destData = stateSnapshot_;
}

void SwingersProcessor::applyLoadedState(PluginState loaded)
{
    state_ = std::move(loaded);
    rebuild();
    notifyChanged();
}

void SwingersProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const auto size = sizeInBytes > 0 ? static_cast<size_t>(sizeInBytes) : size_t { 0 };
    auto loaded = deserialize(data, size);
    if (! loaded)
        return; // not our state: keep what we have

    {
        // An immediate getStateInformation returns what the host just gave us.
        const juce::ScopedLock lock(stateLock_);
        stateSnapshot_ = juce::MemoryBlock(data, size);
    }

    auto* manager = juce::MessageManager::getInstanceWithoutCreating();
    if (manager == nullptr || manager->isThisTheMessageThread())
    {
        applyLoadedState(std::move(*loaded));
        return;
    }

    // Off the message thread: apply there, unless the processor is gone by then.
    juce::MessageManager::callAsync([this, alive = alive_, state = std::move(*loaded)]() mutable {
        if (alive->load())
            applyLoadedState(std::move(state));
    });
}

} // namespace swingers

#ifdef JucePlugin_Name
// Plugin entry point used by the JUCE plugin wrappers (not needed by the test target).
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new swingers::SwingersProcessor();
}
#endif
