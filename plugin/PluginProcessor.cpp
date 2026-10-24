#include "PluginProcessor.h"

namespace swingers
{

SwingersProcessor::SwingersProcessor()
    : juce::AudioProcessor(BusesProperties())
{
}

void SwingersProcessor::prepareToPlay(double, int) {}
void SwingersProcessor::releaseResources() {}

void SwingersProcessor::processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&)
{
    // MIDI passes through untouched for now; the engine is wired in by later tasks.
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

void SwingersProcessor::getStateInformation(juce::MemoryBlock&) {}
void SwingersProcessor::setStateInformation(const void*, int) {}

} // namespace swingers

#ifdef JucePlugin_Name
// Plugin entry point used by the JUCE plugin wrappers (not needed by the test target).
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new swingers::SwingersProcessor();
}
#endif
