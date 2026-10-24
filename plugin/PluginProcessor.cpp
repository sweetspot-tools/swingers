#include "PluginProcessor.h"

namespace swingers
{

SwingersProcessor::SwingersProcessor(juce::File userRoot)
    : juce::AudioProcessor(BusesProperties()), library_(std::move(userRoot))
{
    rebuild();
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
        rebuild();
    else
        rebuild(&overrides, "Could not save the exceptions: " + saved.getErrorMessage());

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
    destData = serialize(state_);
}

void SwingersProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto loaded = deserialize(data, sizeInBytes > 0 ? static_cast<size_t>(sizeInBytes) : 0);
    if (! loaded)
        return; // not our state: keep the defaults / what we have

    state_ = std::move(*loaded);
    rebuild();
    notifyChanged();
}

} // namespace swingers

#ifdef JucePlugin_Name
// Plugin entry point used by the JUCE plugin wrappers (not needed by the test target).
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new swingers::SwingersProcessor();
}
#endif
